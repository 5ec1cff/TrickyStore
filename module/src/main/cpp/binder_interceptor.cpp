#include <utils/RefBase.h>
#include <binder/IPCThreadState.h>
#include <binder/Parcel.h>
#include <binder/IBinder.h>
#include <binder/Binder.h>
#include <utils/StrongPointer.h>
#include <binder/Common.h>
#include <binder/IServiceManager.h>
#include <sys/ioctl.h>
#include "kernel/binder.h"

#include <utility>
#include <map>
#include <shared_mutex>
#include <vector>
#include <queue>

#include "logging.hpp"
#include "dobby.h"
#include "elf_util.h"

#include "hook_util/hook_helper.hpp"

using namespace SandHook;
using namespace android;
using namespace hook_helper::literals;

class BinderInterceptor : public BBinder {
    enum {
        REGISTER_INTERCEPTOR = 1,
        UNREGISTER_INTERCEPTOR = 2
    };
    enum {
        PRE_TRANSACT = 1,
        POST_TRANSACT
    };
    enum {
        SKIP = 1,
        CONTINUE,
        OVERRIDE_REPLY,
        OVERRIDE_DATA
    };
    struct InterceptItem {
        wp<IBinder> target{};
        sp<IBinder> interceptor;
    };
    using RwLock = std::shared_mutex;
    using WriteGuard = std::unique_lock<RwLock>;
    using ReadGuard = std::shared_lock<RwLock>;
    RwLock lock;
    std::map<wp<IBinder>, InterceptItem> items{};
public:
    status_t onTransact(uint32_t code, const android::Parcel &data, android::Parcel *reply,
                        uint32_t flags) override;

    bool handleIntercept(sp<BBinder> target, uint32_t code, const Parcel &data, Parcel *reply,
                         uint32_t flags, status_t &result);

    bool needIntercept(const wp<BBinder>& target);
};

static sp<BinderInterceptor> gBinderInterceptor = nullptr;

struct thread_transaction_info {
    uint32_t code;
    wp<BBinder> target;
};

thread_local std::queue<thread_transaction_info> ttis;

class BinderStub : public BBinder {
    status_t onTransact(uint32_t code, const android::Parcel &data, android::Parcel *reply, uint32_t flags) override {
        LOGD("BinderStub %d", code);
        if (!ttis.empty()) {
            auto tti = ttis.front();
            ttis.pop();
            if (tti.target == nullptr && tti.code == 0xdeadbeef && reply) {
                LOGD("backdoor requested!");
                reply->writeStrongBinder(gBinderInterceptor);
                return OK;
            } else if (tti.target != nullptr) {
                LOGD("intercepting");
                auto p = tti.target.promote();
                if (p) {
                    LOGD("calling interceptor");
                    status_t result;
                    if (!gBinderInterceptor->handleIntercept(p, tti.code, data, reply, flags,
                                                             result)) {
                        LOGD("calling orig");
                        result = p->transact(tti.code, data, reply, flags);
                    }
                    return result;
                } else {
                    LOGE("promote failed");
                }
            }
        }
        return UNKNOWN_TRANSACTION;
    }
};

static sp<BinderStub> gBinderStub = nullptr;

int (*old_ioctl)(int fd, int request, ...) = nullptr;
int new_ioctl(int fd, int request, ...) {
    va_list list;
    va_start(list, request);
    auto arg = va_arg(list, void*);
    va_end(list);
    auto result = old_ioctl(fd, request, arg);
    // TODO: check fd
    if (result >= 0 && request == BINDER_WRITE_READ) {
        auto &bwr = *(struct binder_write_read*) arg;
        LOGD("read buffer %p size %zu consumed %zu", bwr.read_buffer, bwr.read_size,
             bwr.read_consumed);
        if (bwr.read_buffer != 0 && bwr.read_size != 0 && bwr.read_consumed > sizeof(int32_t)) {
            auto ptr = bwr.read_buffer;
            auto consumed = bwr.read_consumed;
            while (consumed > 0) {
                consumed -= sizeof(uint32_t);
                if (consumed < 0) {
                    LOGE("consumed < 0");
                    break;
                }
                auto cmd = *(uint32_t *) ptr;
                ptr += sizeof(uint32_t);
                auto sz = _IOC_SIZE(cmd);
                LOGD("ioctl cmd %d sz %d", cmd, sz);
                consumed -= sz;
                if (consumed < 0) {
                    LOGE("consumed < 0");
                    break;
                }
                if (cmd == BR_TRANSACTION_SEC_CTX || cmd == BR_TRANSACTION) {
                    binder_transaction_data_secctx *tr_secctx = nullptr;
                    binder_transaction_data *tr = nullptr;
                    if (cmd == BR_TRANSACTION_SEC_CTX) {
                        LOGD("cmd is BR_TRANSACTION_SEC_CTX");
                        tr_secctx = (binder_transaction_data_secctx *) ptr;
                        tr = &tr_secctx->transaction_data;
                    } else {
                        LOGD("cmd is BR_TRANSACTION");
                        tr = (binder_transaction_data *) ptr;
                    }

                    if (tr != nullptr) {
                        auto wt = tr->target.ptr;
                        if (wt != 0) {
                            bool need_intercept = false;
                            thread_transaction_info tti{};
                            if (tr->code == 0xdeadbeef && tr->sender_euid == 0) {
                                tti.code = 0xdeadbeef;
                                tti.target = nullptr;
                                need_intercept = true;
                            } else if (reinterpret_cast<RefBase::weakref_type *>(wt)->attemptIncStrong(
                                    nullptr)) {
                                auto b = (BBinder *) tr->cookie;
                                auto wb = wp<BBinder>::fromExisting(b);
                                if (gBinderInterceptor->needIntercept(wb)) {
                                    tti.code = tr->code;
                                    tti.target = wb;
                                    need_intercept = true;
                                    LOGD("intercept code=%d target=%p", tr->code, b);
                                }
                                b->decStrong(nullptr);
                            }
                            if (need_intercept) {
                                LOGD("add intercept item!");
                                tr->target.ptr = (uintptr_t) gBinderStub->getWeakRefs();
                                tr->cookie = (uintptr_t) gBinderStub.get();
                                tr->code = 0xdeadbeef;
                                ttis.push(tti);
                            }
                        }
                    } else {
                        LOGE("no transaction data found!");
                    }
                }
                ptr += sz;
            }
        }
    }
    return result;
}

bool BinderInterceptor::needIntercept(const wp<BBinder> &target) {
    ReadGuard g{lock};
    return items.find(target) != items.end();
}

status_t
BinderInterceptor::onTransact(uint32_t code, const android::Parcel &data, android::Parcel *reply,
                              uint32_t flags) {
    if (code == REGISTER_INTERCEPTOR) {
        sp<IBinder> target, interceptor;
        if (data.readStrongBinder(&target) != OK) {
            return BAD_VALUE;
        }
        if (!target->localBinder()) {
            return BAD_VALUE;
        }
        if (data.readStrongBinder(&interceptor) != OK) {
            return BAD_VALUE;
        }
        {
            WriteGuard wg{lock};
            wp<IBinder> t = target;
            auto it = items.lower_bound(t);
            if (it == items.end() || it->first != t) {
                it = items.emplace_hint(it, t, InterceptItem{});
                it->second.target = t;
            }
            // TODO: send callback to old interceptor
            it->second.interceptor = interceptor;
            return OK;
        }
    } else if (code == UNREGISTER_INTERCEPTOR) {
        sp<IBinder> target, interceptor;
        if (data.readStrongBinder(&target) != OK) {
            return BAD_VALUE;
        }
        if (!target->localBinder()) {
            return BAD_VALUE;
        }
        if (data.readStrongBinder(&interceptor) != OK) {
            return BAD_VALUE;
        }
        {
            WriteGuard wg{lock};
            wp<IBinder> t = target;
            auto it = items.find(t);
            if (it != items.end()) {
                if (it->second.interceptor != interceptor) {
                    return BAD_VALUE;
                }
                items.erase(it);
                return OK;
            }
            return BAD_VALUE;
        }
    }
    return UNKNOWN_TRANSACTION;
}


class HookHandler : public hook_helper::HookHandler {
public:
    ElfImg img;

    explicit HookHandler(ElfInfo info) : img{std::move(info)} {}

    bool isValid() const {
        return img.isValid();
    }

    void *get_symbol(const char *name) const override {
        auto addr = img.getSymbAddress(name);
        if (!addr) {
            LOGE("%s: symbol not found", name);
        }
        return addr;
    }

    void *get_symbol_prefix(const char *prefix) const override {
        auto addr = img.getSymbPrefixFirstOffset(prefix);
        if (!addr) {
            LOGE("%s: prefix symbol not found", prefix);
        }
        return addr;
    }

    void *hook(void *original, void *replacement) const override {
        void *result = nullptr;
        if (DobbyHook(original, (dobby_dummy_func_t) replacement, (dobby_dummy_func_t *) &result) ==
            0) {
            return result;
        } else {
            return nullptr;
        }
    }

    std::pair<uintptr_t, size_t> get_symbol_info(const char *name) const override {
        auto p = img.getSymInfo(name);
        if (!p.first) {
            LOGE("%s: info not found", name);
        }
        return p;
    }
};

bool
BinderInterceptor::handleIntercept(sp<BBinder> target, uint32_t code, const Parcel &data, Parcel *reply,
                                   uint32_t flags, status_t &result) {
#define CHECK(expr) ({ auto __result = (expr); if (__result != OK) { LOGE(#expr " = %d", __result); return false; } })
    sp<IBinder> interceptor;
    {
        ReadGuard rg{lock};
        auto it = items.find(target);
        if (it == items.end()) {
            LOGE("no intercept item found!");
            return false;
        }
        interceptor = it->second.interceptor;
    }
    LOGD("intercept on binder %p code %d flags %d (reply=%s)", target.get(), code, flags,
         reply ? "true" : "false");
    Parcel tmpData, tmpReply, realData;
    CHECK(tmpData.writeStrongBinder(target));
    CHECK(tmpData.writeUint32(code));
    CHECK(tmpData.writeUint32(flags));
    CHECK(tmpData.writeInt32(IPCThreadState::self()->getCallingUid()));
    CHECK(tmpData.writeInt32(IPCThreadState::self()->getCallingPid()));
    CHECK(tmpData.writeUint64(data.dataSize()));
    CHECK(tmpData.appendFrom(&data, 0, data.dataSize()));
    CHECK(interceptor->transact(PRE_TRANSACT, tmpData, &tmpReply));
    int32_t preType;
    CHECK(tmpReply.readInt32(&preType));
    LOGD("pre transact type %d", preType);
    if (preType == SKIP) {
        return false;
    } else if (preType == OVERRIDE_REPLY) {
        result = tmpReply.readInt32();
        if (reply) {
            size_t sz = tmpReply.readUint64();
            CHECK(reply->appendFrom(&tmpReply, tmpReply.dataPosition(), sz));
        }
        return true;
    } else if (preType == OVERRIDE_DATA) {
        size_t sz = tmpReply.readUint64();
        CHECK(realData.appendFrom(&tmpReply, tmpReply.dataPosition(), sz));
    } else {
        CHECK(realData.appendFrom(&data, 0, data.dataSize()));
    }
    result = target->transact(code, realData, reply, flags);

    tmpData.freeData();
    tmpReply.freeData();

    CHECK(tmpData.writeStrongBinder(target));
    CHECK(tmpData.writeUint32(code));
    CHECK(tmpData.writeUint32(flags));
    CHECK(tmpData.writeInt32(IPCThreadState::self()->getCallingUid()));
    CHECK(tmpData.writeInt32(IPCThreadState::self()->getCallingPid()));
    CHECK(tmpData.writeInt32(result));
    CHECK(tmpData.writeUint64(data.dataSize()));
    CHECK(tmpData.appendFrom(&data, 0, data.dataSize()));
    CHECK(tmpData.writeUint64(reply == nullptr ? 0 : reply->dataSize()));
    LOGD("data size %zu reply size %zu", data.dataSize(), reply == nullptr ? 0 : reply->dataSize());
    if (reply) {
        CHECK(tmpData.appendFrom(reply, 0, reply->dataSize()));
    }
    CHECK(interceptor->transact(POST_TRANSACT, tmpData, &tmpReply));
    int32_t postType;
    CHECK(tmpReply.readInt32(&postType));
    LOGD("post transact type %d", postType);
    if (postType == OVERRIDE_REPLY) {
        result = tmpReply.readInt32();
        if (reply) {
            size_t sz = tmpReply.readUint64();
            reply->freeData();
            CHECK(reply->appendFrom(&tmpReply, tmpReply.dataPosition(), sz));
            LOGD("reply size=%zu sz=%zu", reply->dataSize(), sz);
        }
    }
    return true;
}

bool hookBinder() {
    ElfImg img{ElfInfo::getElfInfoForName("libc.so")};
    if (!img.isValid()) {
        LOGE("libbinder not found!");
        return false;
    }
    gBinderInterceptor = sp<BinderInterceptor>::make();
    gBinderStub = sp<BinderStub>::make();
    auto ioctlAddr = img.getSymbAddress("ioctl");
    if (DobbyHook(ioctlAddr, (dobby_dummy_func_t) new_ioctl, (dobby_dummy_func_t*) &old_ioctl) != 0) {
        LOGE("hook failed!");
        return false;
    }
    LOGI("hook success!");
    return true;
}

extern "C" [[gnu::visibility("default")]] [[gnu::used]] bool entry(void *handle) {
    LOGI("my handle %p", handle);
    return hookBinder();
}

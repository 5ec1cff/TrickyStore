#include <utils/RefBase.h>
#include <binder/IPCThreadState.h>
#include <binder/Parcel.h>
#include <binder/IBinder.h>
#include <binder/Binder.h>
#include <utils/StrongPointer.h>
#include <binder/Common.h>

#include <utility>
#include <map>
#include <shared_mutex>
#include <vector>

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

    bool handleIntercept(BBinder *thiz, uint32_t code, const Parcel &data, Parcel *reply,
                         uint32_t flags, status_t &result);
};

static sp<BinderInterceptor> gBinderInterceptor = nullptr;

CREATE_MEM_HOOK_STUB_ENTRY(
        "_ZN7android7BBinder8transactEjRKNS_6ParcelEPS1_j",
        status_t, BBinder_Transact,
        (BBinder * thiz, uint32_t code, const Parcel &data, Parcel *reply, uint32_t flags),
        {
            LOGD("transact: binder=%p code=%d", thiz, code);
            if (IPCThreadState::self()->getCallingUid() == 0 && reply != nullptr &&
                thiz != gBinderInterceptor) [[unlikely]] {
                if (code == 0xdeadbeef) {
                    LOGD("request binder interceptor");
                    reply->writeStrongBinder(gBinderInterceptor);
                    return OK;
                }
            }
            status_t result;
            if (gBinderInterceptor->handleIntercept(thiz, code, data, reply,
                                                               flags, result)) {
                LOGD("transact intercepted: binder=%p code=%d result=%d", thiz, code, result);
                return result;
            }
            result = backup(thiz, code, data, reply, flags);
            LOGD("transact: binder=%p code=%d result=%d", thiz, code, result);
            return result;
        });

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
BinderInterceptor::handleIntercept(BBinder *thiz, uint32_t code, const Parcel &data, Parcel *reply,
                                   uint32_t flags, status_t &result) {
#define CHECK(expr) ({ auto __result = (expr); if (__result != OK) { LOGE(#expr " = %d", __result); return false; } })
    sp<IBinder> interceptor;
    {
        ReadGuard rg{lock};
        wp<IBinder> target = wp<IBinder>::fromExisting(thiz);
        auto it = items.find(target);
        if (it == items.end()) return false;
        interceptor = it->second.interceptor;
    }
    LOGD("intercept on binder %p code %d flags %d (reply=%s)", thiz, code, flags,
         reply ? "true" : "false");
    sp<IBinder> target = sp<IBinder>::fromExisting(thiz);
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
    result = BBinder_Transact.backup(thiz, code, realData, reply, flags);

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
    HookHandler handler{ElfInfo::getElfInfoForName("libbinder.so")};
    if (!handler.isValid()) {
        LOGE("libbinder not found!");
        return false;
    }
    if (!hook_helper::HookSym(handler, BBinder_Transact)) {
        LOGE("hook failed!");
        return false;
    }
    LOGI("hook success!");
    gBinderInterceptor = sp<BinderInterceptor>::make();
    return true;
}

extern "C" [[gnu::visibility("default")]] [[gnu::used]] bool entry(void *handle) {
    LOGI("my handle %p", handle);
    return hookBinder();
}

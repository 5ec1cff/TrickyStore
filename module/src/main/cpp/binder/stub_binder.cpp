#include "binder/IBinder.h"
#include "binder/Binder.h"
#include "binder/BpBinder.h"
#include "binder/IPCThreadState.h"
#include "binder/Parcel.h"
#include "binder/IInterface.h"
#include "binder/IServiceManager.h"

namespace android {
    // IBinder.h
    IBinder::IBinder() {}
    IBinder::~IBinder() {}
    sp<IInterface> IBinder::queryLocalInterface(const String16& descriptor) { return nullptr; }
    bool IBinder::checkSubclass(const void* subclassID) const { return false; }
    void IBinder::withLock(const std::function<void()>& doWithLock) {}

    BBinder* IBinder::localBinder() { return nullptr; }
    BpBinder* IBinder::remoteBinder() { return nullptr; }
    
    // Binder.h
#ifdef __LP64__
    static_assert(sizeof(IBinder) == 24);
    static_assert(sizeof(BBinder) == 40);
#else
    static_assert(sizeof(IBinder) == 12);
    static_assert(sizeof(BBinder) == 20);
#endif
    BBinder::BBinder() {}

    const String16 &BBinder::getInterfaceDescriptor() const { __builtin_unreachable(); }
    bool BBinder::isBinderAlive() const { return false; }
    status_t BBinder::pingBinder() { return 0; }
    status_t BBinder::dump(int fd, const Vector<String16>& args) { return 0; }

    // NOLINTNEXTLINE(google-default-arguments)
    status_t BBinder::transact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) { return 0; }

    // NOLINTNEXTLINE(google-default-arguments)
    status_t BBinder::linkToDeath(const sp<DeathRecipient>& recipient, void* cookie, uint32_t flags) { return 0; }

    // NOLINTNEXTLINE(google-default-arguments)
    status_t BBinder::unlinkToDeath(const wp<DeathRecipient>& recipient, void* cookie, uint32_t flags, wp<DeathRecipient>* outRecipient) { return 0; }

    void* BBinder::attachObject(const void* objectID, void* object, void* cleanupCookie, object_cleanup_func func) { return nullptr; }
    void* BBinder::findObject(const void* objectID) const { return nullptr; }
    void* BBinder::detachObject(const void* objectID) { return nullptr; }
    void BBinder::withLock(const std::function<void()>& doWithLock) {}

    BBinder* BBinder::localBinder() { return nullptr; }
    BBinder::~BBinder() {}

    // NOLINTNEXTLINE(google-default-arguments)
    status_t BBinder::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) { return 0; }
    
    // IPCThreadState.h

    IPCThreadState* IPCThreadState::self() { return nullptr; }
    IPCThreadState* IPCThreadState::selfOrNull() { return nullptr; }

    pid_t IPCThreadState::getCallingPid() const { return 0; }

    const char* IPCThreadState::getCallingSid() const { return nullptr; }

    uid_t IPCThreadState::getCallingUid() const { return 0; }
    
    // Parcel.h
#ifdef __LP64__
    static_assert(sizeof(Parcel) == 120);
#else
    static_assert(sizeof(Parcel) == 60);
#endif
    Parcel::Parcel() {}
    Parcel::~Parcel() {}

    const uint8_t* Parcel::data() const { return nullptr; }
    size_t Parcel::dataSize() const { return 0; }
    size_t Parcel::dataAvail() const { return 0; }
    size_t Parcel::dataPosition() const { return 0; }
    size_t Parcel::dataCapacity() const { return 0; }
    size_t Parcel::dataBufferSize() const { return 0; }

    status_t Parcel::setDataSize(size_t size) { return 0; }

    // this must only be used to set a data position that was previously returned from
    // dataPosition(). If writes are made, the exact same types of writes must be made (e.g.
    // auto i = p.dataPosition(); p.writeInt32(0); p.setDataPosition(i); p.writeInt32(1);).
    // Writing over objects, such as file descriptors and binders, is not supported.
    void Parcel::setDataPosition(size_t pos) const { }
    status_t Parcel::setDataCapacity(size_t size) { return 0; }

    status_t Parcel::setData(const uint8_t* buffer, size_t len) { return 0; }

    status_t Parcel::appendFrom(const Parcel* parcel, size_t start, size_t len) { return 0; }

    // Verify there are no bytes left to be read on the Parcel.
    // Returns Status(EX_BAD_PARCELABLE) when the Parcel is not consumed.
    binder::Status Parcel::enforceNoDataAvail() const { return {}; }

    // This Api is used by fuzzers to skip dataAvail checks.
    void Parcel::setEnforceNoDataAvail(bool enforceNoDataAvail) {}

    void Parcel::freeData() {}

    status_t Parcel::write(const void* data, size_t len) { return 0; }
    void* Parcel::writeInplace(size_t len) { return nullptr; }
    status_t Parcel::writeUnpadded(const void* data, size_t len) { return 0; }
    status_t Parcel::writeInt32(int32_t val) { return 0; }
    status_t Parcel::writeUint32(uint32_t val) { return 0; }
    status_t Parcel::writeInt64(int64_t val) { return 0; }
    status_t Parcel::writeUint64(uint64_t val) { return 0; }
    status_t Parcel::writeFloat(float val) { return 0; }
    status_t Parcel::writeDouble(double val) { return 0; }
    status_t Parcel::writeCString(const char* str) { return 0; }
    status_t Parcel::writeString8(const char* str, size_t len) { return 0; }
    status_t Parcel::writeStrongBinder(const sp<IBinder>& val) { return 0; }
    status_t Parcel::writeBool(bool val) { return 0; }
    status_t Parcel::writeChar(char16_t val) { return 0; }
    status_t Parcel::writeByte(int8_t val) { return 0; }

    // Like Parcel.java's writeNoException().  Just writes a zero int32.
    // Currently the native implementation doesn't do any of the StrictMode
    // stack gathering and serialization that the Java implementation does.
    status_t Parcel::writeNoException() { return 0; }

    status_t Parcel::read(void* outData, size_t len) const { return 0; }
    const void* Parcel::readInplace(size_t len) const { return nullptr; }
    int32_t Parcel::readInt32() const { return 0; }
    status_t Parcel::readInt32(int32_t* pArg) const { return 0; }
    uint32_t Parcel::readUint32() const { return 0; }
    status_t Parcel::readUint32(uint32_t* pArg) const { return 0; }
    int64_t Parcel::readInt64() const { return 0; }
    status_t Parcel::readInt64(int64_t* pArg) const { return 0; }
    uint64_t Parcel::readUint64() const { return 0; }
    status_t Parcel::readUint64(uint64_t* pArg) const { return 0; }
    float Parcel::readFloat() const { return 0; }
    status_t Parcel::readFloat(float* pArg) const { return 0; }
    double Parcel::readDouble() const { return 0; }
    status_t Parcel::readDouble(double* pArg) const { return 0; }
    bool Parcel::readBool() const { return 0; }
    status_t Parcel::readBool(bool* pArg) const { return 0; }
    char16_t Parcel::readChar() const { return 0; }
    status_t Parcel::readChar(char16_t* pArg) const { return 0; }
    int8_t Parcel::readByte() const { return 0; }
    status_t Parcel::readByte(int8_t* pArg) const { return 0; }

    sp<IBinder> Parcel::readStrongBinder() const { return nullptr; }
    status_t Parcel::readStrongBinder(sp<IBinder>* val) const { return 0; }
    status_t Parcel::readNullableStrongBinder(sp<IBinder>* val) const { return 0; }

    int32_t Parcel::readExceptionCode() const { return 0; }
    int Parcel::readFileDescriptor() const { return 0; }

    // IServiceManager.h
    const String16 &IServiceManager::getInterfaceDescriptor() const {
        __builtin_unreachable();
    }

    IServiceManager::IServiceManager() {}

    IServiceManager::~IServiceManager() {}

    sp<IServiceManager> defaultServiceManager() { return nullptr; }

    void setDefaultServiceManager(const sp<IServiceManager> &sm) {}
}

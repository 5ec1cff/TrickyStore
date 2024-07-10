#pragma once

#include <array>
#include <limits>
#include <map> // for legacy reasons
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <binder/unique_fd.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/String16.h>
#include <utils/Vector.h>

#include <binder/Common.h>

//NOLINTNEXTLINE(google-runtime-int) b/173188702
typedef unsigned long long binder_size_t;

// ---------------------------------------------------------------------------
namespace android {
    class IBinder;
    namespace binder {
        class Status {};
    }

    class Parcel {
    public:

        LIBBINDER_EXPORTED Parcel();
        LIBBINDER_EXPORTED ~Parcel();

        LIBBINDER_EXPORTED const uint8_t* data() const;
        LIBBINDER_EXPORTED size_t dataSize() const;
        LIBBINDER_EXPORTED size_t dataAvail() const;
        LIBBINDER_EXPORTED size_t dataPosition() const;
        LIBBINDER_EXPORTED size_t dataCapacity() const;
        LIBBINDER_EXPORTED size_t dataBufferSize() const;

        LIBBINDER_EXPORTED status_t setDataSize(size_t size);

        // this must only be used to set a data position that was previously returned from
        // dataPosition(). If writes are made, the exact same types of writes must be made (e.g.
        // auto i = p.dataPosition(); p.writeInt32(0); p.setDataPosition(i); p.writeInt32(1);).
        // Writing over objects, such as file descriptors and binders, is not supported.
        LIBBINDER_EXPORTED void setDataPosition(size_t pos) const;
        LIBBINDER_EXPORTED status_t setDataCapacity(size_t size);

        LIBBINDER_EXPORTED status_t setData(const uint8_t* buffer, size_t len);

        LIBBINDER_EXPORTED status_t appendFrom(const Parcel* parcel, size_t start, size_t len);

        // Verify there are no bytes left to be read on the Parcel.
        // Returns Status(EX_BAD_PARCELABLE) when the Parcel is not consumed.
        LIBBINDER_EXPORTED binder::Status enforceNoDataAvail() const;

        // This Api is used by fuzzers to skip dataAvail checks.
        LIBBINDER_EXPORTED void setEnforceNoDataAvail(bool enforceNoDataAvail);

        LIBBINDER_EXPORTED void freeData();

        LIBBINDER_EXPORTED status_t write(const void* data, size_t len);
        LIBBINDER_EXPORTED void* writeInplace(size_t len);
        LIBBINDER_EXPORTED status_t writeUnpadded(const void* data, size_t len);
        LIBBINDER_EXPORTED status_t writeInt32(int32_t val);
        LIBBINDER_EXPORTED status_t writeUint32(uint32_t val);
        LIBBINDER_EXPORTED status_t writeInt64(int64_t val);
        LIBBINDER_EXPORTED status_t writeUint64(uint64_t val);
        LIBBINDER_EXPORTED status_t writeFloat(float val);
        LIBBINDER_EXPORTED status_t writeDouble(double val);
        LIBBINDER_EXPORTED status_t writeCString(const char* str);
        LIBBINDER_EXPORTED status_t writeString8(const char* str, size_t len);
        LIBBINDER_EXPORTED status_t writeStrongBinder(const sp<IBinder>& val);
        LIBBINDER_EXPORTED status_t writeBool(bool val);
        LIBBINDER_EXPORTED status_t writeChar(char16_t val);
        LIBBINDER_EXPORTED status_t writeByte(int8_t val);

        // Like Parcel.java's writeNoException().  Just writes a zero int32.
        // Currently the native implementation doesn't do any of the StrictMode
        // stack gathering and serialization that the Java implementation does.
        LIBBINDER_EXPORTED status_t writeNoException();

        LIBBINDER_EXPORTED status_t read(void* outData, size_t len) const;
        LIBBINDER_EXPORTED const void* readInplace(size_t len) const;
        LIBBINDER_EXPORTED int32_t readInt32() const;
        LIBBINDER_EXPORTED status_t readInt32(int32_t* pArg) const;
        LIBBINDER_EXPORTED uint32_t readUint32() const;
        LIBBINDER_EXPORTED status_t readUint32(uint32_t* pArg) const;
        LIBBINDER_EXPORTED int64_t readInt64() const;
        LIBBINDER_EXPORTED status_t readInt64(int64_t* pArg) const;
        LIBBINDER_EXPORTED uint64_t readUint64() const;
        LIBBINDER_EXPORTED status_t readUint64(uint64_t* pArg) const;
        LIBBINDER_EXPORTED float readFloat() const;
        LIBBINDER_EXPORTED status_t readFloat(float* pArg) const;
        LIBBINDER_EXPORTED double readDouble() const;
        LIBBINDER_EXPORTED status_t readDouble(double* pArg) const;
        LIBBINDER_EXPORTED bool readBool() const;
        LIBBINDER_EXPORTED status_t readBool(bool* pArg) const;
        LIBBINDER_EXPORTED char16_t readChar() const;
        LIBBINDER_EXPORTED status_t readChar(char16_t* pArg) const;
        LIBBINDER_EXPORTED int8_t readByte() const;
        LIBBINDER_EXPORTED status_t readByte(int8_t* pArg) const;

        LIBBINDER_EXPORTED sp<IBinder> readStrongBinder() const;
        LIBBINDER_EXPORTED status_t readStrongBinder(sp<IBinder>* val) const;
        LIBBINDER_EXPORTED status_t readNullableStrongBinder(sp<IBinder>* val) const;

        // Like Parcel.java's readExceptionCode().  Reads the first int32
        // off of a Parcel's header, returning 0 or the negative error
        // code on exceptions, but also deals with skipping over rich
        // response headers.  Callers should use this to read & parse the
        // response headers rather than doing it by hand.
        LIBBINDER_EXPORTED int32_t readExceptionCode() const;

        // Retrieve a file descriptor from the parcel.  This returns the raw fd
        // in the parcel, which you do not own -- use dup() to get your own copy.
        LIBBINDER_EXPORTED int readFileDescriptor() const;

    private:
        // `objects` and `objectsSize` always 0 for RPC Parcels.
        typedef void (*release_func)(const uint8_t* data, size_t dataSize, const binder_size_t* objects,
                                     size_t objectsSize);

        // special int32 value to indicate NonNull or Null parcelables
        // This is fixed to be only 0 or 1 by contract, do not change.
        static constexpr int32_t kNonNullParcelableFlag = 1;
        static constexpr int32_t kNullParcelableFlag = 0;

        // special int32 size representing a null vector, when applicable in Nullable data.
        // This fixed as -1 by contract, do not change.
        static constexpr int32_t kNullVectorSize = -1;

        //-----------------------------------------------------------------------------
    private:

        status_t            mError;
        uint8_t*            mData;
        size_t              mDataSize;
        size_t              mDataCapacity;
        mutable size_t mDataPos;

        // Fields only needed when parcelling for "kernel Binder".
        struct KernelFields {
            KernelFields() {}
            binder_size_t* mObjects = nullptr;
            size_t mObjectsSize = 0;
            size_t mObjectsCapacity = 0;
            mutable size_t mNextObjectHint = 0;

            mutable size_t mWorkSourceRequestHeaderPosition = 0;
            mutable bool mRequestHeaderPresent = false;

            mutable bool mObjectsSorted = false;
            mutable bool mFdsKnown = true;
            mutable bool mHasFds = false;
        };
        // Fields only needed when parcelling for RPC Binder.
        struct RpcFields {};
        std::variant<KernelFields, RpcFields> mVariantFields;

        // Pointer to KernelFields in mVariantFields if present.
        KernelFields* maybeKernelFields() { return std::get_if<KernelFields>(&mVariantFields); }
        const KernelFields* maybeKernelFields() const {
            return std::get_if<KernelFields>(&mVariantFields);
        }
        // Pointer to RpcFields in mVariantFields if present.
        RpcFields* maybeRpcFields() { return std::get_if<RpcFields>(&mVariantFields); }
        const RpcFields* maybeRpcFields() const { return std::get_if<RpcFields>(&mVariantFields); }

        bool                mAllowFds;

        // if this parcelable is involved in a secure transaction, force the
        // data to be overridden with zero when deallocated
        mutable bool        mDeallocZero;

        // Set this to false to skip dataAvail checks.
        bool mEnforceNoDataAvail;
        bool mServiceFuzzing;

        release_func        mOwner;

        size_t mReserved;
    };

} // namespace android

// ---------------------------------------------------------------------------
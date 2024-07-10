/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <binder/Common.h>
#include <binder/unique_fd.h>
#include <utils/Errors.h>
#include <utils/RefBase.h>
#include <utils/String16.h>
#include <utils/Vector.h>

#include <functional>

// linux/binder.h defines this, but we don't want to include it here in order to
// avoid exporting the kernel headers
#ifndef B_PACK_CHARS
#define B_PACK_CHARS(c1, c2, c3, c4) \
    ((((c1)<<24)) | (((c2)<<16)) | (((c3)<<8)) | (c4))
#endif  // B_PACK_CHARS

// ---------------------------------------------------------------------------
namespace android {

    class BBinder;
    class BpBinder;
    class IInterface;
    class Parcel;

/**
 * Base class and low-level protocol for a remotable object.
 * You can derive from this class to create an object for which other
 * processes can hold references to it.  Communication between processes
 * (method calls, property get and set) is down through a low-level
 * protocol implemented on top of the transact() API.
 */
    class LIBBINDER_EXPORTED IBinder : public virtual RefBase {
    public:
    enum {
        FIRST_CALL_TRANSACTION = 0x00000001,
        LAST_CALL_TRANSACTION = 0x00ffffff,

        PING_TRANSACTION = B_PACK_CHARS('_', 'P', 'N', 'G'),
        START_RECORDING_TRANSACTION = B_PACK_CHARS('_', 'S', 'R', 'D'),
        STOP_RECORDING_TRANSACTION = B_PACK_CHARS('_', 'E', 'R', 'D'),
        DUMP_TRANSACTION = B_PACK_CHARS('_', 'D', 'M', 'P'),
        SHELL_COMMAND_TRANSACTION = B_PACK_CHARS('_', 'C', 'M', 'D'),
        INTERFACE_TRANSACTION = B_PACK_CHARS('_', 'N', 'T', 'F'),
        SYSPROPS_TRANSACTION = B_PACK_CHARS('_', 'S', 'P', 'R'),
        EXTENSION_TRANSACTION = B_PACK_CHARS('_', 'E', 'X', 'T'),
        DEBUG_PID_TRANSACTION = B_PACK_CHARS('_', 'P', 'I', 'D'),
        SET_RPC_CLIENT_TRANSACTION = B_PACK_CHARS('_', 'R', 'P', 'C'),

        // See android.os.IBinder.TWEET_TRANSACTION
        // Most importantly, messages can be anything not exceeding 130 UTF-8
        // characters, and callees should exclaim "jolly good message old boy!"
        TWEET_TRANSACTION = B_PACK_CHARS('_', 'T', 'W', 'T'),

        // See android.os.IBinder.LIKE_TRANSACTION
        // Improve binder self-esteem.
        LIKE_TRANSACTION = B_PACK_CHARS('_', 'L', 'I', 'K'),

        // Corresponds to TF_ONE_WAY -- an asynchronous call.
        FLAG_ONEWAY = 0x00000001,

        // Corresponds to TF_CLEAR_BUF -- clear transaction buffers after call
        // is made
        FLAG_CLEAR_BUF = 0x00000020,

        // Private userspace flag for transaction which is being requested from
        // a vendor context.
        FLAG_PRIVATE_VENDOR = 0x10000000,
    };

    IBinder();

    /**
     * Check if this IBinder implements the interface named by
     * @a descriptor.  If it does, the base pointer to it is returned,
     * which you can safely static_cast<> to the concrete C++ interface.
     */
    virtual sp<IInterface>  queryLocalInterface(const String16& descriptor);

    /**
     * Return the canonical name of the interface provided by this IBinder
     * object.
     */
    virtual const String16& getInterfaceDescriptor() const = 0;

    /**
     * Last known alive status, from last call. May be arbitrarily stale.
     * May be incorrect if a service returns an incorrect status code.
     */
    virtual bool            isBinderAlive() const = 0;
    virtual status_t        pingBinder() = 0;
    virtual status_t        dump(int fd, const Vector<String16>& args) = 0;

    // NOLINTNEXTLINE(google-default-arguments)
    virtual status_t        transact(   uint32_t code,
                                        const Parcel& data,
                                        Parcel* reply,
                                        uint32_t flags = 0) = 0;

    // DeathRecipient is pure abstract, there is no virtual method
    // implementation to put in a translation unit in order to silence the
    // weak vtables warning.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wweak-vtables"
#endif

    class DeathRecipient : public virtual RefBase
    {
    public:
        virtual void binderDied(const wp<IBinder>& who) = 0;
    };

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

    /**
     * Register the @a recipient for a notification if this binder
     * goes away.  If this binder object unexpectedly goes away
     * (typically because its hosting process has been killed),
     * then DeathRecipient::binderDied() will be called with a reference
     * to this.
     *
     * The @a cookie is optional -- if non-NULL, it should be a
     * memory address that you own (that is, you know it is unique).
     *
     * @note When all references to the binder being linked to are dropped, the
     * recipient is automatically unlinked. So, you must hold onto a binder in
     * order to receive death notifications about it.
     *
     * @note You will only receive death notifications for remote binders,
     * as local binders by definition can't die without you dying as well.
     * Trying to use this function on a local binder will result in an
     * INVALID_OPERATION code being returned and nothing happening.
     *
     * @note This link always holds a weak reference to its recipient.
     *
     * @note You will only receive a weak reference to the dead
     * binder.  You should not try to promote this to a strong reference.
     * (Nor should you need to, as there is nothing useful you can
     * directly do with it now that it has passed on.)
     */
    // NOLINTNEXTLINE(google-default-arguments)
    virtual status_t        linkToDeath(const sp<DeathRecipient>& recipient,
                                        void* cookie = nullptr,
                                        uint32_t flags = 0) = 0;

    /**
     * Remove a previously registered death notification.
     * The @a recipient will no longer be called if this object
     * dies.  The @a cookie is optional.  If non-NULL, you can
     * supply a NULL @a recipient, and the recipient previously
     * added with that cookie will be unlinked.
     *
     * If the binder is dead, this will return DEAD_OBJECT. Deleting
     * the object will also unlink all death recipients.
     */
    // NOLINTNEXTLINE(google-default-arguments)
    virtual status_t        unlinkToDeath(  const wp<DeathRecipient>& recipient,
                                            void* cookie = nullptr,
                                            uint32_t flags = 0,
                                            wp<DeathRecipient>* outRecipient = nullptr) = 0;

    virtual bool            checkSubclass(const void* subclassID) const;

    typedef void (*object_cleanup_func)(const void* id, void* obj, void* cleanupCookie);

    /**
     * This object is attached for the lifetime of this binder object. When
     * this binder object is destructed, the cleanup function of all attached
     * objects are invoked with their respective objectID, object, and
     * cleanupCookie. Access to these APIs can be made from multiple threads,
     * but calls from different threads are allowed to be interleaved.
     *
     * This returns the object which is already attached. If this returns a
     * non-null value, it means that attachObject failed (a given objectID can
     * only be used once).
     */
    [[nodiscard]] virtual void* attachObject(const void* objectID, void* object,
                                             void* cleanupCookie, object_cleanup_func func) = 0;
    /**
     * Returns object attached with attachObject.
     */
    [[nodiscard]] virtual void* findObject(const void* objectID) const = 0;
    /**
     * Returns object attached with attachObject, and detaches it. This does not
     * delete the object.
     */
    [[nodiscard]] virtual void* detachObject(const void* objectID) = 0;

    /**
     * Use the lock that this binder contains internally. For instance, this can
     * be used to modify an attached object without needing to add an additional
     * lock (though, that attached object must be retrieved before calling this
     * method). Calling (most) IBinder methods inside this will deadlock.
     */
    void withLock(const std::function<void()>& doWithLock);

    virtual BBinder*        localBinder();
    virtual BpBinder*       remoteBinder();

    protected:
    virtual          ~IBinder();

    private:
};

} // namespace android

// ---------------------------------------------------------------------------

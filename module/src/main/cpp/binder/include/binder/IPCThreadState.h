/*
 * Copyright (C) 2005 The Android Open Source Project
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
#include <binder/Parcel.h>
#include <utils/Errors.h>
#include <utils/Vector.h>

// ---------------------------------------------------------------------------
namespace android {

/**
 * Kernel binder thread state. All operations here refer to kernel binder. This
 * object is allocated per-thread.
 */
    class IPCThreadState {
    public:

        LIBBINDER_EXPORTED static IPCThreadState* self();
        LIBBINDER_EXPORTED static IPCThreadState* selfOrNull(); // self(), but won't instantiate

        [[nodiscard]] LIBBINDER_EXPORTED pid_t getCallingPid() const;

        /**
         * Returns the SELinux security identifier of the process which has
         * made the current binder call. If not in a binder call this will
         * return nullptr. If this isn't requested with
         * Binder::setRequestingSid, it will also return nullptr.
         *
         * This can't be restored once it's cleared, and it does not return the
         * context of the current process when not in a binder call.
         */
        [[nodiscard]] LIBBINDER_EXPORTED const char* getCallingSid() const;

        /**
         * Returns the UID of the process which has made the current binder
         * call. If not in a binder call, this will return 0.
         */
        [[nodiscard]] LIBBINDER_EXPORTED uid_t getCallingUid() const;
    };

} // namespace android

// ---------------------------------------------------------------------------
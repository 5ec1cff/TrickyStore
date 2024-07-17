#include "utils/StrongPointer.h"
#include "utils/RefBase.h"
#include "utils/String16.h"

namespace android {
    void RefBase::incStrong(const void *id) const {

    }

    void RefBase::incStrongRequireStrong(const void *id) const {

    }

    void RefBase::decStrong(const void *id) const {

    }

    void RefBase::forceIncStrong(const void *id) const {

    }

    RefBase::weakref_type* RefBase::createWeak(const void* id) const {
        return nullptr;
    }

    RefBase::weakref_type* RefBase::getWeakRefs() const {
        return nullptr;
    }

    RefBase::RefBase(): mRefs(nullptr) {}
    RefBase::~RefBase() {}

    void RefBase::onFirstRef() {}
    void RefBase::onLastStrongRef(const void* id) {}
    bool RefBase::onIncStrongAttempted(uint32_t flags, const void* id) { return false; }
    void RefBase::onLastWeakRef(const void* id) {}

    RefBase* RefBase::weakref_type::refBase() const { return nullptr; }

    void RefBase::weakref_type::incWeak(const void* id) {}
    void RefBase::weakref_type::incWeakRequireWeak(const void* id) {}
    void RefBase::weakref_type::decWeak(const void* id) {}

    // acquires a strong reference if there is already one.
    bool RefBase::weakref_type::attemptIncStrong(const void* id) { return false; }

    // acquires a weak reference if there is already one.
    // This is not always safe. see ProcessState.cpp and BpBinder.cpp
    // for proper use.
    bool RefBase::weakref_type::attemptIncWeak(const void* id) { return false; }

    void sp_report_race() {}

    String16::String16() {}

    String16::String16(const String16 &o) {}

    String16::String16(String16 &&o) noexcept {}

    String16::String16(const char *o) {}

    String16::~String16() {}
}

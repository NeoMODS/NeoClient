#include "game/jvm.h"

#include "core/logger.h"

#include <windows.h>

namespace neo {
namespace {

using GetCreatedJavaVMsFn = jint(JNICALL*)(JavaVM**, jsize, jsize*);

} // namespace

bool Jvm::attach() {
    if (vm_) {
        return true;
    }

    HMODULE jvmModule = GetModuleHandleW(L"jvm.dll");
    if (!jvmModule) {
        return false;
    }

    auto getCreated = reinterpret_cast<GetCreatedJavaVMsFn>(
        GetProcAddress(jvmModule, "JNI_GetCreatedJavaVMs"));
    if (!getCreated) {
        NEO_LOG_ERROR("jvm.dll does not export JNI_GetCreatedJavaVMs");
        return false;
    }

    JavaVM* vms[1]{};
    jsize count = 0;
    if (getCreated(vms, 1, &count) != JNI_OK || count < 1) {
        return false;
    }

    vm_ = vms[0];
    return true;
}

JNIEnv* Jvm::env() const {
    if (!vm_ && !const_cast<Jvm*>(this)->attach()) {
        return nullptr;
    }

    const unsigned long threadId = static_cast<unsigned long>(GetCurrentThreadId());
    if (env_ && envThreadId_ == threadId) {
        return env_;
    }

    JNIEnv* environment = nullptr;
    const jint result = vm_->GetEnv(reinterpret_cast<void**>(&environment), JNI_VERSION_1_8);
    if (result == JNI_EDETACHED) {
        if (vm_->AttachCurrentThreadAsDaemon(reinterpret_cast<void**>(&environment), nullptr) != JNI_OK) {
            NEO_LOG_ERROR("Could not attach the overlay thread to the JVM");
            return nullptr;
        }
        NEO_LOG_INFO("Attached overlay thread to JVM");
    } else if (result != JNI_OK) {
        NEO_LOG_ERROR("GetEnv failed with %d", static_cast<int>(result));
        return nullptr;
    }

    env_ = environment;
    envThreadId_ = threadId;
    return environment;
}

} // namespace neo

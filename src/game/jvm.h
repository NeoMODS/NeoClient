#pragma once

#include <jni.h>

namespace neo {

class Jvm {
public:
    bool attach();

    // Returns the JNIEnv of the calling thread. Native threads are attached
    // on demand; a Java thread always gets its own environment. Caching the
    // environment per thread is required because JNIEnv is thread local and
    // the overlay can run its present hook on more than one thread.
    JNIEnv* env() const;

    JavaVM* vm() const { return vm_; }
    bool ok() const { return vm_ != nullptr; }

private:
    mutable JavaVM* vm_ = nullptr;
    mutable JNIEnv* env_ = nullptr;
    mutable unsigned long envThreadId_ = 0;
};

} // namespace neo

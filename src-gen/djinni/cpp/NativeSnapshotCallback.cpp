// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from rocketspeed.djinni

#include "src-gen/djinni/cpp/NativeSnapshotCallback.hpp"  // my header
#include "src-gen/djinni/cpp/NativeStatus.hpp"

namespace djinni_generated {

NativeSnapshotCallback::NativeSnapshotCallback() : ::djinni::JniInterface<::rocketspeed::djinni::SnapshotCallback, NativeSnapshotCallback>() {}

NativeSnapshotCallback::~NativeSnapshotCallback() = default;

NativeSnapshotCallback::JavaProxy::JavaProxy(JniType j) : JavaProxyCacheEntry(j) { }

NativeSnapshotCallback::JavaProxy::~JavaProxy() = default;

void NativeSnapshotCallback::JavaProxy::Call(::rocketspeed::djinni::Status status) {
    auto jniEnv = ::djinni::jniGetThreadEnv();
    ::djinni::JniLocalScope jscope(jniEnv, 10);
    const auto& data = ::djinni::JniClass<::djinni_generated::NativeSnapshotCallback>::get();
    jniEnv->CallVoidMethod(getGlobalRef(), data.method_call,
                           ::djinni_generated::NativeStatus::fromCpp(jniEnv, status).get());
    ::djinni::jniExceptionCheck(jniEnv);
}

}  // namespace djinni_generated

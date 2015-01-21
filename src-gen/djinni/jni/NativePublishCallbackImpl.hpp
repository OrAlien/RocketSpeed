// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from rocketspeed.djinni

#pragma once

#include "PublishCallbackImpl.hpp"
#include "djinni_support.hpp"

namespace djinni_generated {

class NativePublishCallbackImpl final : djinni::JniInterface<::rocketspeed::djinni::PublishCallbackImpl, NativePublishCallbackImpl> {
public:
    using CppType = std::shared_ptr<::rocketspeed::djinni::PublishCallbackImpl>;
    using JniType = jobject;

    static jobject toJava(JNIEnv* jniEnv, std::shared_ptr<::rocketspeed::djinni::PublishCallbackImpl> c) { return djinni::JniClass<::djinni_generated::NativePublishCallbackImpl>::get()._toJava(jniEnv, c); }
    static std::shared_ptr<::rocketspeed::djinni::PublishCallbackImpl> fromJava(JNIEnv* jniEnv, jobject j) { return djinni::JniClass<::djinni_generated::NativePublishCallbackImpl>::get()._fromJava(jniEnv, j); }

    const djinni::GlobalRef<jclass> clazz { djinni::jniFindClass("org/rocketspeed/PublishCallbackImpl") };
    const jmethodID method_Call { djinni::jniGetMethodID(clazz.get(), "Call", "(Lorg/rocketspeed/Status;ILjava/lang/String;Lorg/rocketspeed/MsgIdImpl;J)V") };

    class JavaProxy final : djinni::JavaProxyCacheEntry, public ::rocketspeed::djinni::PublishCallbackImpl {
    public:
        JavaProxy(jobject obj);
        virtual void Call(::rocketspeed::djinni::Status status, int32_t namespace_id, std::string topic_name, ::rocketspeed::djinni::MsgIdImpl message_id, int64_t sequence_number) override;

    private:
        using djinni::JavaProxyCacheEntry::getGlobalRef;
        friend class djinni::JniInterface<::rocketspeed::djinni::PublishCallbackImpl, ::djinni_generated::NativePublishCallbackImpl>;
        friend class djinni::JavaProxyCache<JavaProxy>;
    };

private:
    NativePublishCallbackImpl();
    friend class djinni::JniClass<::djinni_generated::NativePublishCallbackImpl>;
};

}  // namespace djinni_generated

// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from rocketspeed.djinni

#include "NativeSubscriptionRequestImpl.hpp"  // my header
#include "HBool.hpp"
#include "HI32.hpp"
#include "HI64.hpp"
#include "HOptional.hpp"
#include "HString.hpp"

namespace djinni_generated {

jobject NativeSubscriptionRequestImpl::toJava(JNIEnv* jniEnv, ::rocketspeed::djinni::SubscriptionRequestImpl c) {
    jint j_namespace_id = ::djinni::HI32::Unboxed::toJava(jniEnv, c.namespace_id);
    djinni::LocalRef<jstring> j_topic_name(jniEnv, ::djinni::HString::toJava(jniEnv, c.topic_name));
    jboolean j_subscribe = ::djinni::HBool::Unboxed::toJava(jniEnv, c.subscribe);
    djinni::LocalRef<jobject> j_start(jniEnv, ::djinni::HOptional<std::experimental::optional, ::djinni::HI64>::toJava(jniEnv, c.start));
    const auto & data = djinni::JniClass<::djinni_generated::NativeSubscriptionRequestImpl>::get();
    jobject r = jniEnv->NewObject(data.clazz.get(), data.jconstructor, j_namespace_id, j_topic_name.get(), j_subscribe, j_start.get());
    djinni::jniExceptionCheck(jniEnv);
    return r;
}

::rocketspeed::djinni::SubscriptionRequestImpl NativeSubscriptionRequestImpl::fromJava(JNIEnv* jniEnv, jobject j) {
    assert(j != nullptr);
    const auto & data = djinni::JniClass<::djinni_generated::NativeSubscriptionRequestImpl>::get();
    return ::rocketspeed::djinni::SubscriptionRequestImpl(
        ::djinni::HI32::Unboxed::fromJava(jniEnv, jniEnv->GetIntField(j, data.field_namespaceId)),
        ::djinni::HString::fromJava(jniEnv, djinni::LocalRef<jstring>(jniEnv, static_cast<jstring>(jniEnv->GetObjectField(j, data.field_topicName))).get()),
        ::djinni::HBool::Unboxed::fromJava(jniEnv, jniEnv->GetBooleanField(j, data.field_subscribe)),
        ::djinni::HOptional<std::experimental::optional, ::djinni::HI64>::fromJava(jniEnv, djinni::LocalRef<jobject>(jniEnv, jniEnv->GetObjectField(j, data.field_start)).get()));
}

}  // namespace djinni_generated

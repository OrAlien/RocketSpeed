// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from rocketspeed.djinni

#pragma once

#include "ConfigurationImpl.hpp"
#include "MsgIdImpl.hpp"
#include "PublishStatus.hpp"
#include "RetentionBase.hpp"
#include "SubscriptionRequestImpl.hpp"
#include "SubscriptionStorage.hpp"
#include "optional.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace rocketspeed { namespace djinni {

class PublishCallbackImpl;
class ReceiveCallbackImpl;
class SubscribeCallbackImpl;

class ClientImpl {
public:
    virtual ~ClientImpl() {}

    static std::shared_ptr<ClientImpl> Open(ConfigurationImpl config, std::string client_id, std::shared_ptr<SubscribeCallbackImpl> subscribe_callback, std::shared_ptr<ReceiveCallbackImpl> receive_callback, SubscriptionStorage storage);

    virtual PublishStatus Publish(int32_t namespace_id, std::string topic_name, RetentionBase retention, std::vector<uint8_t> data, std::experimental::optional<MsgIdImpl> message_id, std::shared_ptr<PublishCallbackImpl> publish_callback) = 0;

    virtual void ListenTopics(std::vector<SubscriptionRequestImpl> names) = 0;

    virtual void Acknowledge(int32_t namespace_id, std::string topic_name, int64_t sequence_number) = 0;

    virtual void Close() = 0;
};

} }  // namespace rocketspeed::djinni

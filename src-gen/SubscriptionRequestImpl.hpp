// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from rocketspeed.djinni

#pragma once

#include "SubscriptionStartImpl.hpp"
#include <cstdint>
#include <string>
#include <utility>

namespace rocketspeed { namespace djinni {

struct SubscriptionRequestImpl final {

    int16_t namespace_id;

    std::string topic_name;

    bool subscribe;

    SubscriptionStartImpl sequence_number;


    SubscriptionRequestImpl(
            int16_t namespace_id,
            std::string topic_name,
            bool subscribe,
            SubscriptionStartImpl sequence_number) :
                namespace_id(std::move(namespace_id)),
                topic_name(std::move(topic_name)),
                subscribe(std::move(subscribe)),
                sequence_number(std::move(sequence_number)) {
    }
};

} }  // namespace rocketspeed::djinni
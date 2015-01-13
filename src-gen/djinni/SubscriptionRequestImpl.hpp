// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from rocketspeed.djinni

#pragma once

#include "optional.hpp"
#include <cstdint>
#include <string>
#include <utility>

namespace rocketspeed { namespace djinni {

struct SubscriptionRequestImpl final {

    int16_t namespace_id;

    std::string topic_name;

    bool subscribe;

    std::experimental::optional<int64_t> start;


    SubscriptionRequestImpl(
            int16_t namespace_id,
            std::string topic_name,
            bool subscribe,
            std::experimental::optional<int64_t> start) :
                namespace_id(std::move(namespace_id)),
                topic_name(std::move(topic_name)),
                subscribe(std::move(subscribe)),
                start(std::move(start)) {
    }
};

} }  // namespace rocketspeed::djinni

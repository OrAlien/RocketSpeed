// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from rocketspeed.djinni

#pragma once

#include <functional>

namespace rocketspeed { namespace djinni {

enum class StorageType : int {
    NONE,
    FILE,
};

} }  // namespace rocketspeed::djinni

namespace std {

template <>
struct hash<::rocketspeed::djinni::StorageType> {
    size_t operator()(::rocketspeed::djinni::StorageType type) const {
        return std::hash<int>()(static_cast<int>(type));
    }
};

}  // namespace std

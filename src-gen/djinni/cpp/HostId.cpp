// AUTOGENERATED FILE - DO NOT MODIFY!
// This file generated by Djinni from rocketspeed.djinni

#include "src-gen/djinni/cpp/HostId.hpp"  // my header

namespace rocketspeed { namespace djinni {


bool operator==(const HostId& lhs, const HostId& rhs) {
    return lhs.host == rhs.host &&
           lhs.port == rhs.port;
}

bool operator!=(const HostId& lhs, const HostId& rhs) {
    return !(lhs == rhs);
}

} }  // namespace rocketspeed::djinni

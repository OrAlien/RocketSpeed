// Copyright (c) 2015, Facebook, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
//
#include "host_id.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <string>

#include "include/Slice.h"
#include "include/Status.h"
#include "src/util/common/hash.h"
#include "src/util/common/parsing.h"

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

namespace rocketspeed {

Status HostId::Resolve(const std::string& str, HostId* out) {
  auto host_and_port = SplitString(str, ':');
  if (host_and_port.size() != 2) {
    return Status::InvalidArgument("Malformed or missing port");
  }
  const auto& port_str = host_and_port[1];
  char* end_ptr;
  long int port_val = strtol(port_str.c_str(), &end_ptr, 10);
  if (port_val < 0 || port_val > 65535 ||
      end_ptr != port_str.c_str() + port_str.length()) {
    return Status::InvalidArgument("Invalid port: " + port_str);
  }
  return Resolve(host_and_port[0], static_cast<uint16_t>(port_val), out);
}

Status HostId::Resolve(const std::string& hostname,
                       uint16_t port,
                       HostId* out) {
  // Allow getaddrinfo to perform DNS resolution if necessary.
  return ResolveInternal(hostname, port, AI_NUMERICSERV, out);
}

Status HostId::CreateFromIP(const std::string& ip_address,
                            uint16_t port,
                            HostId* out) {
  // The address must be valid, resolved IP address, we do not fall back to DNS
  // resolution.
  return ResolveInternal(
      ip_address, port, AI_NUMERICHOST | AI_NUMERICSERV, out);
}

HostId HostId::CreateLocal(uint16_t port, std::string description) {
  sockaddr_in loopback;
  memset(&loopback, 0, sizeof(loopback));
  loopback.sin_family = AF_INET;
  loopback.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  loopback.sin_port = htons(port);

  if (description.empty()) {
    description = "localhost:" + std::to_string(port);
  }

  const sockaddr* addr = reinterpret_cast<sockaddr*>(&loopback);
  socklen_t addrlen = static_cast<socklen_t>(sizeof(loopback));
  return HostId(addr, addrlen, std::move(description));
}

HostId::HostId() : addrlen_(0) {
  memset(&storage_, 0, sizeof(storage_));
}

bool HostId::operator<(const HostId& rhs) const {
  return memcmp(&storage_, &rhs.storage_, sizeof(storage_)) < 0;
}

bool HostId::operator==(const HostId& rhs) const {
  return memcmp(&storage_, &rhs.storage_, sizeof(storage_)) == 0;
}

size_t HostId::Hash() const {
  return MurmurHash2<Slice>()(
      Slice(reinterpret_cast<const char*>(&storage_), sizeof(storage_)));
}

Status HostId::ResolveInternal(const std::string& hostname,
                               uint16_t port,
                               int ai_flags,
                               HostId* out) {
  addrinfo hints;
  memset(&hints, 0, sizeof hints);
  hints.ai_flags = ai_flags;
  // Accept both IPv4 and IPv6 addresses.
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  std::string port_str(std::to_string(port));
  addrinfo* result = nullptr;
  if (int rv =
          getaddrinfo(hostname.c_str(), port_str.c_str(), &hints, &result)) {
    return Status::IOError("Failed to resolve " + hostname + ":" + port_str +
                           ", reason: " + gai_strerror(rv));
  }

  if (!result) {
    return Status::IOError("No resolution for " + hostname + ":" + port_str);
  }

  // RFC 3484: IPv6 address will precede IPv4 address on the list returned by
  // getaddrinfo.
  *out = HostId(result->ai_addr, result->ai_addrlen, hostname + ":" + port_str);

  freeaddrinfo(result);
  return Status::OK();
}

HostId::HostId(const sockaddr* addr, socklen_t addrlen, std::string description)
: addrlen_(addrlen), description_(std::move(description)) {
  assert(addrlen_ > 0);
  assert(sizeof(storage_) >= addrlen_);
  memset(&storage_, 0, sizeof(storage_));
  memcpy(&storage_, addr, addrlen_);
}
}  // namespace rocketspeed

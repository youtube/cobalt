// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_endpoint.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <winsock2.h>
#include <ws2bth.h>
#elif defined(OS_POSIX)
#include <netinet/in.h>
#endif

#include <tuple>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "net/base/ip_address.h"

#if defined(OS_WIN)
#include "net/base/winsock_util.h"
#endif

#if defined(STARBOARD)
#include "starboard/memory.h"
#include "starboard/types.h"
#endif

namespace net {

#if defined(STARBOARD)
bool GetIPAddressFromSbSocketAddress(const SbSocketAddress* address,
                                     const unsigned char** out_address_data,
                                     size_t* out_address_len,
                                     uint16_t* out_port) {
  DCHECK(address);
  DCHECK(out_address_data);
  DCHECK(out_address_len);
  if (out_port) {
    *out_port = address->port;
  }

  *out_address_data = address->address;
  switch (address->type) {
    case kSbSocketAddressTypeIpv4:
      *out_address_len = IPAddress::kIPv4AddressSize;
      break;
    case kSbSocketAddressTypeIpv6:
      *out_address_len = IPAddress::kIPv6AddressSize;
      break;

    default:
      NOTREACHED();
      return false;
  }

  return true;
}
#else  // defined(STARBOARD)
namespace {

// By definition, socklen_t is large enough to hold both sizes.
const socklen_t kSockaddrInSize = sizeof(struct sockaddr_in);
const socklen_t kSockaddrIn6Size = sizeof(struct sockaddr_in6);

// Extracts the address and port portions of a sockaddr.
bool GetIPAddressFromSockAddr(const struct sockaddr* sock_addr,
                              socklen_t sock_addr_len,
                              const uint8_t** address,
                              size_t* address_len,
                              uint16_t* port) {
  if (sock_addr->sa_family == AF_INET) {
    if (sock_addr_len < static_cast<socklen_t>(sizeof(struct sockaddr_in)))
      return false;
    const struct sockaddr_in* addr =
        reinterpret_cast<const struct sockaddr_in*>(sock_addr);
    *address = reinterpret_cast<const uint8_t*>(&addr->sin_addr);
    *address_len = IPAddress::kIPv4AddressSize;
    if (port)
      *port = base::NetToHost16(addr->sin_port);
    return true;
  }

  if (sock_addr->sa_family == AF_INET6) {
    if (sock_addr_len < static_cast<socklen_t>(sizeof(struct sockaddr_in6)))
      return false;
    const struct sockaddr_in6* addr =
        reinterpret_cast<const struct sockaddr_in6*>(sock_addr);
    *address = reinterpret_cast<const uint8_t*>(&addr->sin6_addr);
    *address_len = IPAddress::kIPv6AddressSize;
    if (port)
      *port = base::NetToHost16(addr->sin6_port);
    return true;
  }

#if defined(OS_WIN)
  if (sock_addr->sa_family == AF_BTH) {
    if (sock_addr_len < static_cast<socklen_t>(sizeof(SOCKADDR_BTH)))
      return false;
    const SOCKADDR_BTH* addr = reinterpret_cast<const SOCKADDR_BTH*>(sock_addr);
    *address = reinterpret_cast<const uint8_t*>(&addr->btAddr);
    *address_len = kBluetoothAddressSize;
    if (port)
      *port = static_cast<uint16_t>(addr->port);
    return true;
  }
#endif

  return false;  // Unrecognized |sa_family|.
}

}  // namespace

#endif  // defined(STARBOARD)

IPEndPoint::IPEndPoint() : port_(0) {}

IPEndPoint::~IPEndPoint() = default;

IPEndPoint::IPEndPoint(const IPAddress& address, uint16_t port)
    : address_(address), port_(port) {}

IPEndPoint::IPEndPoint(const IPEndPoint& endpoint) {
  address_ = endpoint.address_;
  port_ = endpoint.port_;
}

AddressFamily IPEndPoint::GetFamily() const {
  return GetAddressFamily(address_);
}

#if !defined(STARBOARD)
int IPEndPoint::GetSockAddrFamily() const {
  switch (address_.size()) {
    case IPAddress::kIPv4AddressSize:
      return AF_INET;
    case IPAddress::kIPv6AddressSize:
      return AF_INET6;
    default:
      NOTREACHED() << "Bad IP address";
      return AF_UNSPEC;
  }
}
#endif  // !defined(STARBOARD)

#if defined(STARBOARD)
// static
IPEndPoint IPEndPoint::GetForAllInterfaces(int port) {
  // Directly construct the 0.0.0.0 address with the given port.
  IPAddress address(0, 0, 0, 0);
  return IPEndPoint(address, port);
}

bool IPEndPoint::ToSbSocketAddress(SbSocketAddress* out_address) const {
  DCHECK(out_address);
  out_address->port = port_;
  memset(out_address->address, 0, sizeof(out_address->address));
  switch (GetFamily()) {
    case ADDRESS_FAMILY_IPV4:
      out_address->type = kSbSocketAddressTypeIpv4;
      memcpy(&out_address->address, address_.bytes().data(),
                   IPAddress::kIPv4AddressSize);
      break;
    case ADDRESS_FAMILY_IPV6:
      out_address->type = kSbSocketAddressTypeIpv6;
      memcpy(&out_address->address, address_.bytes().data(),
                   IPAddress::kIPv6AddressSize);
      break;
    default:
      NOTREACHED();
      return false;
  }
  return true;
}

bool IPEndPoint::FromSbSocketAddress(const SbSocketAddress* address) {
  DCHECK(address);

  const uint8_t* address_data;
  size_t address_len;
  uint16_t port;
  if (!GetIPAddressFromSbSocketAddress(address, &address_data, &address_len,
                                       &port)) {
    return false;
  }

  address_ = net::IPAddress(address_data, address_len);
  port_ = port;
  return true;
}
#else  // defined(STARBOARD)
bool IPEndPoint::ToSockAddr(struct sockaddr* address,
                            socklen_t* address_length) const {
  DCHECK(address);
  DCHECK(address_length);
  switch (address_.size()) {
    case IPAddress::kIPv4AddressSize: {
      if (*address_length < kSockaddrInSize)
        return false;
      *address_length = kSockaddrInSize;
      struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(address);
      memset(addr, 0, sizeof(struct sockaddr_in));
      addr->sin_family = AF_INET;
      addr->sin_port = base::HostToNet16(port_);
      memcpy(&addr->sin_addr, address_.bytes().data(),
                   IPAddress::kIPv4AddressSize);
      break;
    }
    case IPAddress::kIPv6AddressSize: {
      if (*address_length < kSockaddrIn6Size)
        return false;
      *address_length = kSockaddrIn6Size;
      struct sockaddr_in6* addr6 =
          reinterpret_cast<struct sockaddr_in6*>(address);
      memset(addr6, 0, sizeof(struct sockaddr_in6));
      addr6->sin6_family = AF_INET6;
      addr6->sin6_port = base::HostToNet16(port_);
      memcpy(&addr6->sin6_addr, address_.bytes().data(),
                   IPAddress::kIPv6AddressSize);
      break;
    }
    default:
      return false;
  }
  return true;
}

bool IPEndPoint::FromSockAddr(const struct sockaddr* sock_addr,
                              socklen_t sock_addr_len) {
  DCHECK(sock_addr);

  const uint8_t* address;
  size_t address_len;
  uint16_t port;
  if (!GetIPAddressFromSockAddr(sock_addr, sock_addr_len, &address,
                                &address_len, &port)) {
    return false;
  }

  address_ = net::IPAddress(address, address_len);
  port_ = port;
  return true;
}
#endif  // defined(STARBOARD)

std::string IPEndPoint::ToString() const {
  return IPAddressToStringWithPort(address_, port_);
}

std::string IPEndPoint::ToStringWithoutPort() const {
  return address_.ToString();
}

bool IPEndPoint::operator<(const IPEndPoint& other) const {
  // Sort IPv4 before IPv6.
  if (address_.size() != other.address_.size()) {
    return address_.size() < other.address_.size();
  }
  return std::tie(address_, port_) < std::tie(other.address_, other.port_);
}

bool IPEndPoint::operator==(const IPEndPoint& other) const {
  return address_ == other.address_ && port_ == other.port_;
}

}  // namespace net

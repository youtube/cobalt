// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_endpoint.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/sys_byteorder.h"
#if defined(OS_WIN)
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <netinet/in.h>
#endif

namespace net {

namespace {
// By definition, socklen_t is large enough to hold both sizes.
const socklen_t kSockaddrInSize = sizeof(struct sockaddr_in);
const socklen_t kSockaddrIn6Size = sizeof(struct sockaddr_in6);
}

IPEndPoint::IPEndPoint() : port_(0) {}

IPEndPoint::~IPEndPoint() {}

IPEndPoint::IPEndPoint(const IPAddressNumber& address, int port)
    : address_(address),
      port_(port) {}

IPEndPoint::IPEndPoint(const IPEndPoint& endpoint) {
  address_ = endpoint.address_;
  port_ = endpoint.port_;
}

int IPEndPoint::GetFamily() const {
  switch (address_.size()) {
    case kIPv4AddressSize:
      return AF_INET;
    case kIPv6AddressSize:
      return AF_INET6;
    default:
      NOTREACHED() << "Bad IP address";
      return AF_UNSPEC;
  }
}

bool IPEndPoint::ToSockAddr(struct sockaddr* address,
                            socklen_t* address_length) const {
  DCHECK(address);
  DCHECK(address_length);
  switch (address_.size()) {
    case kIPv4AddressSize: {
      if (*address_length < kSockaddrInSize)
        return false;
      *address_length = kSockaddrInSize;
      struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(address);
      memset(addr, 0, sizeof(struct sockaddr_in));
      addr->sin_family = AF_INET;
      addr->sin_port = base::HostToNet16(port_);
      memcpy(&addr->sin_addr, &address_[0], kIPv4AddressSize);
      break;
    }
    case kIPv6AddressSize: {
      if (*address_length < kSockaddrIn6Size)
        return false;
      *address_length = kSockaddrIn6Size;
      struct sockaddr_in6* addr6 =
          reinterpret_cast<struct sockaddr_in6*>(address);
      memset(addr6, 0, sizeof(struct sockaddr_in6));
      addr6->sin6_family = AF_INET6;
      addr6->sin6_port = base::HostToNet16(port_);
      memcpy(&addr6->sin6_addr, &address_[0], kIPv6AddressSize);
      break;
    }
    default:
      return false;
  }
  return true;
}

bool IPEndPoint::FromSockAddr(const struct sockaddr* address,
                              socklen_t address_length) {
  DCHECK(address);
  switch (address->sa_family) {
    case AF_INET: {
      if (address_length < kSockaddrInSize)
        return false;
      const struct sockaddr_in* addr =
          reinterpret_cast<const struct sockaddr_in*>(address);
      port_ = base::NetToHost16(addr->sin_port);
      const char* bytes = reinterpret_cast<const char*>(&addr->sin_addr);
      address_.assign(&bytes[0], &bytes[kIPv4AddressSize]);
      break;
    }
    case AF_INET6: {
      if (address_length < kSockaddrIn6Size)
        return false;
      const struct sockaddr_in6* addr =
          reinterpret_cast<const struct sockaddr_in6*>(address);
      port_ = base::NetToHost16(addr->sin6_port);
      const char* bytes = reinterpret_cast<const char*>(&addr->sin6_addr);
      address_.assign(&bytes[0], &bytes[kIPv6AddressSize]);
      break;
    }
    default:
      return false;
  }
  return true;
}

std::string IPEndPoint::ToString() const {
  SockaddrStorage storage;
  if (!ToSockAddr(storage.addr, &storage.addr_len)) {
    return std::string();
  }
  // TODO(szym): Don't use getnameinfo. http://crbug.com/126212
  return NetAddressToStringWithPort(storage.addr, storage.addr_len);
}

std::string IPEndPoint::ToStringWithoutPort() const {
  SockaddrStorage storage;
  if (!ToSockAddr(storage.addr, &storage.addr_len)) {
    return std::string();
  }
  // TODO(szym): Don't use getnameinfo. http://crbug.com/126212
  return NetAddressToString(storage.addr, storage.addr_len);
}

bool IPEndPoint::operator<(const IPEndPoint& that) const {
  // Sort IPv4 before IPv6.
  if (address_.size() != that.address_.size()) {
    return address_.size() < that.address_.size();
  }
  if (address_ != that.address_) {
    return address_ < that.address_;
  }
  return port_ < that.port_;
}

bool IPEndPoint::operator==(const IPEndPoint& that) const {
  return address_ == that.address_ && port_ == that.port_;
}

}  // namespace net

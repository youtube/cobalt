// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_endpoint.h"

#include "base/logging.h"
#if defined(OS_WIN)
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <netinet/in.h>
#endif

namespace net {

const size_t kIPv4AddressSize = 4;
const size_t kIPv6AddressSize = 16;

IPEndPoint::IPEndPoint() : port_(0) {}

IPEndPoint::~IPEndPoint() {}

IPEndPoint::IPEndPoint(const IPAddressNumber& address, int port)
    : address_(address),
      port_(port) {}

IPEndPoint::IPEndPoint(const IPEndPoint& endpoint) {
  address_ = endpoint.address_;
  port_ = endpoint.port_;
}

bool IPEndPoint::ToSockaddr(struct sockaddr* address,
                            size_t* address_length) const {
  DCHECK(address);
  DCHECK(address_length);
  switch (address_.size()) {
    case kIPv4AddressSize: {
      if (*address_length < sizeof(struct sockaddr_in))
        return false;
      *address_length = sizeof(struct sockaddr_in);
      struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(address);
      memset(addr, 0, sizeof(struct sockaddr_in));
      addr->sin_family = AF_INET;
      addr->sin_port = htons(port_);
      memcpy(&addr->sin_addr, &address_[0], kIPv4AddressSize);
      break;
    }
    case kIPv6AddressSize: {
      if (*address_length < sizeof(struct sockaddr_in6))
        return false;
      *address_length = sizeof(struct sockaddr_in6);
      struct sockaddr_in6* addr6 =
          reinterpret_cast<struct sockaddr_in6*>(address);
      memset(addr6, 0, sizeof(struct sockaddr_in6));
      addr6->sin6_family = AF_INET6;
      addr6->sin6_port = htons(port_);
      memcpy(&addr6->sin6_addr, &address_[0], kIPv6AddressSize);
      break;
    }
    default: {
      NOTREACHED() << "Bad IP address";
      break;
    }
  }
  return true;
}

bool IPEndPoint::FromSockAddr(const struct sockaddr* address,
                              size_t address_length) {
  DCHECK(address);
  switch (address->sa_family) {
    case AF_INET: {
      const struct sockaddr_in* addr =
          reinterpret_cast<const struct sockaddr_in*>(address);
      port_ = ntohs(addr->sin_port);
      const char* bytes = reinterpret_cast<const char*>(&addr->sin_addr);
      address_.assign(&bytes[0], &bytes[kIPv4AddressSize]);
      break;
    }
    case AF_INET6: {
      const struct sockaddr_in6* addr =
          reinterpret_cast<const struct sockaddr_in6*>(address);
      port_ = ntohs(addr->sin6_port);
      const char* bytes = reinterpret_cast<const char*>(&addr->sin6_addr);
      address_.assign(&bytes[0], &bytes[kIPv6AddressSize]);
      break;
    }
    default: {
      NOTREACHED() << "Bad IP address";
      break;
    }
  }
  return true;
}

bool IPEndPoint::operator<(const IPEndPoint& that) const {
  return address_ < that.address_ || port_ < that.port_;
}

bool IPEndPoint::operator==(const IPEndPoint& that) const {
  return address_ == that.address_ && port_ == that.port_;
}

}  // namespace net

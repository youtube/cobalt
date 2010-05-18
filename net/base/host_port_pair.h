// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_PORT_PAIR_H_
#define NET_BASE_HOST_PORT_PAIR_H_

#include <string>
#include "base/basictypes.h"

namespace net {

struct HostPortPair {
  HostPortPair();
  // If |in_host| represents an IPv6 address, it should not bracket the address.
  HostPortPair(const std::string& in_host, uint16 in_port);

  // TODO(willchan): Define a functor instead.
  // Comparator function so this can be placed in a std::map.
  // TODO(jar): Violation of style guide, and should be removed.
  bool operator<(const HostPortPair& other) const {
    if (port != other.port)
      return port < other.port;
    return host < other.host;
  }

  // Equality test of contents. (Probably another violation of style guide).
  bool Equals(const HostPortPair& other) const {
    return host == other.host && port == other.port;
  }

  // ToString() will convert the HostPortPair to "host:port".  If |host| is an
  // IPv6 literal, it will add brackets around |host|.
  std::string ToString() const;

  // If |host| represents an IPv6 address, this string will not contain brackets
  // around the address.
  std::string host;
  uint16 port;
};

}  // namespace net

#endif  // NET_BASE_HOST_PORT_PAIR_H_

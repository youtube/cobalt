// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_HOST_PORT_PAIR_H_
#define NET_BASE_HOST_PORT_PAIR_H_

#include <string>
#include "base/basictypes.h"

namespace net {

struct HostPortPair {
  HostPortPair() {}
  HostPortPair(const std::string& in_host, uint16 in_port)
      : host(in_host), port(in_port) {}

  // Comparator function so this can be placed in a std::map.
  bool operator<(const HostPortPair& other) const {
    if (host != other.host)
      return host < other.host;
    return port < other.port;
  }

  std::string ToString() const;

  std::string host;
  uint16 port;
};

}  // namespace net

#endif  // NET_BASE_HOST_PORT_PAIR_H_

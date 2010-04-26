// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_port_pair.h"
#include "base/string_util.h"

namespace net {

HostPortPair::HostPortPair() : port(0) {}
HostPortPair::HostPortPair(const std::string& in_host, uint16 in_port)
    : host(in_host), port(in_port) {}

std::string HostPortPair::ToString() const {
  // Check to see if the host is an IPv6 address.  If so, added brackets.
  if (host.find(':') != std::string::npos)
    return StringPrintf("[%s]:%u", host.c_str(), port);
  return StringPrintf("%s:%u", host.c_str(), port);
}

}  // namespace net

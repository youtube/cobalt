// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_port_pair.h"

#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "googleurl/src/gurl.h"
#include "net/base/ip_endpoint.h"

namespace net {

HostPortPair::HostPortPair() : port_(0) {}
HostPortPair::HostPortPair(const std::string& in_host, uint16 in_port)
    : host_(in_host), port_(in_port) {}

// static
HostPortPair HostPortPair::FromURL(const GURL& url) {
  return HostPortPair(url.HostNoBrackets(), url.EffectiveIntPort());
}

// static
HostPortPair HostPortPair::FromIPEndPoint(const IPEndPoint& ipe) {
  return HostPortPair(ipe.ToStringWithoutPort(), ipe.port());
}

HostPortPair HostPortPair::FromString(const std::string& str) {
  std::vector<std::string> key_port;
  base::SplitString(str, ':', &key_port);
  if (key_port.size() != 2)
    return HostPortPair();
  int port;
  if (!base::StringToInt(key_port[1], &port))
    return HostPortPair();
  DCHECK_LT(port, 1 << 16);
  HostPortPair host_port_pair;
  host_port_pair.set_host(key_port[0]);
  host_port_pair.set_port(port);
  return host_port_pair;
}

std::string HostPortPair::ToString() const {
  return base::StringPrintf("%s:%u", HostForURL().c_str(), port_);
}

std::string HostPortPair::HostForURL() const {
  // Check to see if the host is an IPv6 address.  If so, added brackets.
  if (host_.find(':') != std::string::npos) {
    DCHECK_NE(host_[0], '[');
    return base::StringPrintf("[%s]", host_.c_str());
  }

  return host_;
}

}  // namespace net

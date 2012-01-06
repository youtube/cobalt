// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_port_pair.h"

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"

namespace net {

HostPortPair::HostPortPair() : port_(0) {}
HostPortPair::HostPortPair(const std::string& in_host, uint16 in_port)
    : host_(in_host), port_(in_port) {}

// static
HostPortPair HostPortPair::FromURL(const GURL& url) {
  return HostPortPair(url.HostNoBrackets(), url.EffectiveIntPort());
}

// static
HostPortPair HostPortPair::FromAddrInfo(const struct addrinfo* ai) {
  return HostPortPair(NetAddressToString(ai),
                      GetPortFromSockaddr(ai->ai_addr, ai->ai_addrlen));
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

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_config_service.h"

#include "net/base/ip_endpoint.h"

namespace net {

// Default values are taken from glibc resolv.h.
DnsConfig::DnsConfig()
  : ndots(1),
    timeout(base::TimeDelta::FromSeconds(5)),
    attempts(2),
    rotate(false),
    edns0(false) {}

DnsConfig::~DnsConfig() {}

bool DnsConfig::Equals(const DnsConfig& d) const {
  return (nameservers == d.nameservers) &&
         (search == d.search) &&
         (ndots == d.ndots) &&
         (timeout == d.timeout) &&
         (attempts == d.attempts) &&
         (rotate == d.rotate) &&
         (edns0 == d.edns0);
}

}  // namespace net


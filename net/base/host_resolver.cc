// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_resolver.h"

namespace net {

HostResolver::RequestInfo::RequestInfo(const HostPortPair& host_port_pair)
    : host_port_pair_(host_port_pair),
      address_family_(ADDRESS_FAMILY_UNSPECIFIED),
      host_resolver_flags_(0),
      allow_cached_response_(true),
      is_speculative_(false),
      priority_(MEDIUM) {
}

HostResolver::~HostResolver() {
}

AddressFamily HostResolver::GetDefaultAddressFamily() const {
  return ADDRESS_FAMILY_UNSPECIFIED;
}

void HostResolver::ProbeIPv6Support() {
}

HostCache* HostResolver::GetHostCache() {
  return NULL;
}

base::Value* HostResolver::GetDnsConfigAsValue() const {
  return NULL;
}

HostResolver::HostResolver() {
}

}  // namespace net

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_FAMILY_H_
#define NET_BASE_ADDRESS_FAMILY_H_

namespace net {

// Enum wrapper around the address family types supported by host resolver
// procedures.
enum AddressFamily {
  ADDRESS_FAMILY_UNSPECIFIED,   // AF_UNSPEC
  ADDRESS_FAMILY_IPV4,          // AF_INET
  ADDRESS_FAMILY_IPV6,          // AF_INET6
};

// HostResolverFlags is a bitflag enum wrapper around the addrinfo.ai_flags
// supported by host resolver procedures.
enum {
  HOST_RESOLVER_CANONNAME = 1 << 0,  // 0x1, AI_CANONNAME
};
typedef int HostResolverFlags;

}  // namespace net

#endif  // NET_BASE_ADDRESS_FAMILY_H_

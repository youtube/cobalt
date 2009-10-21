// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_ADDRESS_FAMILY_H_
#define NET_BASE_ADDRESS_FAMILY_H_

namespace net {

// Enum wrapper around the address family types supported by host resolver
// procedures. These correspond with AF_UNSPEC and AF_INET.
enum AddressFamily {
  ADDRESS_FAMILY_UNSPECIFIED,
  ADDRESS_FAMILY_IPV4_ONLY,
};

}  // namesapce net

#endif  // NET_BASE_ADDRESS_FAMILY_H_

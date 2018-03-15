// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/network/local_network.h"

#include "base/logging.h"
#include "base/string_piece.h"
#include "net/base/net_util.h"

namespace cobalt {
namespace network {

namespace {

bool CompareNBytesOfAddress(const SbSocketAddress& ip,
                            const SbSocketAddress& source_address,
                            const SbSocketAddress& netmask,
                            size_t number_bytes_to_compare) {
  for (size_t i = 0; i != number_bytes_to_compare; ++i) {
    if ((ip.address[i] & netmask.address[i]) !=
        (source_address.address[i] & netmask.address[i])) {
      return false;
    }
  }
  return true;
}

bool IsLocalIP(const SbSocketAddress& ip, const SbSocketAddress& source_address,
               const SbSocketAddress& netmask) {
  DCHECK(source_address.type == ip.type);
  DCHECK(netmask.type == ip.type);

  switch (ip.type) {
    case kSbSocketAddressTypeIpv4:
      return CompareNBytesOfAddress(ip, source_address, netmask,
                                    net::kIPv4AddressSize);
    case kSbSocketAddressTypeIpv6:
#if SB_HAS(IPV6)
      return CompareNBytesOfAddress(ip, source_address, netmask,
                                    net::kIPv6AddressSize);
#else   //  SB_HAS(IPV6)
      NOTREACHED() << "Invalid IP type " << ip.type;
#endif  //  SB_HAS(IPV6)
  }
  return false;
}

}  // namespace

bool IsIPInLocalNetwork(const SbSocketAddress& destination) {
  SbSocketAddress source_address;
  SbSocketAddress netmask;
  if (!(SbSocketGetInterfaceAddress(&destination, &source_address, &netmask))) {
    return false;
  }
  return IsLocalIP(destination, source_address, netmask);
}

bool IsIPInPrivateRange(const SbSocketAddress& ip) {
  // For IPv4, the private address range is defined by RFC 1918
  // avaiable at https://tools.ietf.org/html/rfc1918#section-3.
  if (ip.type == kSbSocketAddressTypeIpv4) {
    if (ip.address[0] == 10) {
      // IP is in range 10.0.0.0 - 10.255.255.255 (10/8 prefix).
      return true;
    }
    if ((ip.address[0] == 192) && (ip.address[1] == 168)) {
      // IP is in range 192.168.0.0 - 192.168.255.255 (192.168/16 prefix).
      return true;
    }
    if ((ip.address[0] == 172) &&
        ((ip.address[1] >= 16) || (ip.address[1] <= 31))) {
      // IP is in range 172.16.0.0 - 172.31.255.255 (172.16/12 prefix).
      return true;
    }
  }
#if SB_HAS(IPV6)
  if (ip.type == kSbSocketAddressTypeIpv6) {
    // Unique Local Addresses for IPv6 are _effectively_ fd00::/8.
    // See https://tools.ietf.org/html/rfc4193#section-3 for details.
    return ip.address[0] == 0xfd && ip.address[1] == 0;
  }
#endif

  return false;
}

}  // namespace network
}  // namespace cobalt

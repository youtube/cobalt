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

#include "cobalt/network/socket_address_parser.h"

#include "base/logging.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_canon_ip.h"
#include "net/base/net_util.h"
#include "starboard/memory.h"

namespace cobalt {
namespace network {

bool ParseSocketAddress(const char* spec,
                        const url_parse::Component& host_component,
                        SbSocketAddress* out_socket_address) {
  DCHECK(out_socket_address);

  unsigned char address_v4[net::kIPv4AddressSize];
  int num_ipv4_components = 0;

  // IPv4AddressToNumber will return either IPV4, BROKEN, or NEUTRAL.  If the
  // input IP address is IPv6, then NEUTRAL will be returned, and a different
  // function will be used to covert the hostname to IPv6 address.
  // If the return value is IPV4, then address will be in network byte order.
  url_canon::CanonHostInfo::Family family = url_canon::IPv4AddressToNumber(
      spec, host_component, address_v4, &num_ipv4_components);

  switch (family) {
    case url_canon::CanonHostInfo::IPV4:
      if (num_ipv4_components != net::kIPv4AddressSize) {
        return false;
      }

      SbMemorySet(out_socket_address, 0, sizeof(SbSocketAddress));
      out_socket_address->type = kSbSocketAddressTypeIpv4;
      DCHECK_GE(sizeof(address_v4),
                static_cast<std::size_t>(num_ipv4_components));
      SbMemoryCopy(out_socket_address->address, address_v4,
                   num_ipv4_components);

      return true;
    case url_canon::CanonHostInfo::NEUTRAL:
#if SB_HAS(IPV6)
      unsigned char address_v6[net::kIPv6AddressSize];
      if (!url_canon::IPv6AddressToNumber(spec, host_component, address_v6)) {
        break;
      }

      SbMemorySet(out_socket_address, 0, sizeof(SbSocketAddress));
      out_socket_address->type = kSbSocketAddressTypeIpv6;
      COMPILE_ASSERT(sizeof(address_v6), kIPv6AddressLength);
      SbMemoryCopy(out_socket_address->address, address_v6, sizeof(address_v6));
      return true;
#else
      return false;
#endif
    case url_canon::CanonHostInfo::BROKEN:
      break;
    case url_canon::CanonHostInfo::IPV6:
      NOTREACHED() << "Invalid return value from IPv4AddressToNumber.";
      break;
    default:
      NOTREACHED() << "Unexpected return value from IPv4AddressToNumber: "
                   << family;
      break;
  }

  return false;
}

}  // namespace network
}  // namespace cobalt

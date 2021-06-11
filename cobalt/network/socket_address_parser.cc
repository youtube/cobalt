// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/base/ip_address.h"
#include "starboard/memory.h"
#include "url/third_party/mozilla/url_parse.h"
#include "url/url_canon.h"
#include "url/url_canon_ip.h"

namespace cobalt {
namespace network {

bool ParseSocketAddress(const char* spec, const url::Component& host_component,
                        SbSocketAddress* out_socket_address) {
  DCHECK(out_socket_address);

  unsigned char address_v4[net::IPAddress::kIPv4AddressSize];
  int num_ipv4_components = 0;

  // IPv4AddressToNumber will return either IPV4, BROKEN, or NEUTRAL.  If the
  // input IP address is IPv6, then NEUTRAL will be returned, and a different
  // function will be used to covert the hostname to IPv6 address.
  // If the return value is IPV4, then address will be in network byte order.
  url::CanonHostInfo::Family family = url::IPv4AddressToNumber(
      spec, host_component, address_v4, &num_ipv4_components);

  switch (family) {
    case url::CanonHostInfo::IPV4:
      if (num_ipv4_components != net::IPAddress::kIPv4AddressSize) {
        return false;
      }

      memset(out_socket_address, 0, sizeof(SbSocketAddress));
      out_socket_address->type = kSbSocketAddressTypeIpv4;
      DCHECK_GE(sizeof(address_v4),
                static_cast<std::size_t>(num_ipv4_components));
      memcpy(out_socket_address->address, address_v4,
                   num_ipv4_components);

      return true;
    case url::CanonHostInfo::NEUTRAL:
#if SB_API_VERSION >= 12
      if (!SbSocketIsIpv6Supported()) {
        return false;
      }
#endif
#if SB_API_VERSION >= 12 || SB_HAS(IPV6)
      unsigned char address_v6[net::IPAddress::kIPv6AddressSize];
      if (!url::IPv6AddressToNumber(spec, host_component, address_v6)) {
        break;
      }

      memset(out_socket_address, 0, sizeof(SbSocketAddress));
      out_socket_address->type = kSbSocketAddressTypeIpv6;
      static_assert(sizeof(address_v6) == net::IPAddress::kIPv6AddressSize, "");
      memcpy(out_socket_address->address, address_v6, sizeof(address_v6));
      return true;
#else
      return false;
#endif
    case url::CanonHostInfo::BROKEN:
      break;
    case url::CanonHostInfo::IPV6:
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

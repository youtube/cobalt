// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/local_ip.h"

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "net/base/ip_address.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/ip_address_space_util.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "third_party/blink/public/platform/platform.h"

namespace blink {

namespace {

std::optional<net::IPAddress> ExtractIPv4From6to4(const net::IPAddress& addr) {
  // Check if it's IPv6 and starts with the 6to4 prefix: 20:02 (0x2002)
  constexpr uint8_t k6to4PrefixUpperByte = 0x20;
  constexpr uint8_t k6to4PrefixLowerByte = 0x02;

  const auto bytes = addr.bytes();
  if (!addr.IsIPv6() || bytes[0] != k6to4PrefixUpperByte || bytes[1] != k6to4PrefixLowerByte) {
    return std::nullopt;
  }

  // Extract the 4 bytes starting at index 2 for the embedded IPv4 address.
  return net::IPAddress(bytes[2], bytes[3], bytes[4], bytes[5]);
}

}  // namespace

// Queries host IP address and matches against netmask of target ip to determine.
bool IsIPInLocalNetwork(const std::string& target_ip_str) {
    if (target_ip_str == "localhost") {
        return true;
    }
    struct ifaddrs* ifaddr;
    if (getifaddrs(&ifaddr) == -1) {
        return false;
    }
    bool is_local = false;
    struct sockaddr_in target_addr;
    if (inet_pton(AF_INET, target_ip_str.c_str(), &target_addr.sin_addr) <= 0) {
        LOG(ERROR) << "Invalid IPv4 address: " << target_ip_str << std::endl;
        freeifaddrs(ifaddr);
        return false;
    }

    for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in* interface_addr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_addr);
            struct sockaddr_in* netmask_addr = reinterpret_cast<struct sockaddr_in*>(ifa->ifa_netmask);
            if (interface_addr == nullptr || netmask_addr == nullptr) {
                continue;
            }
            uint32_t target_ip = target_addr.sin_addr.s_addr;
            uint32_t interface_ip = interface_addr->sin_addr.s_addr;
            uint32_t netmask = netmask_addr->sin_addr.s_addr;
            if ((target_ip & netmask) == (interface_ip & netmask)) {
                is_local = true;
                break;
            }
        }
    }

    freeifaddrs(ifaddr);
    return is_local;
}

bool IsIPInPrivateRange(const std::string& raw_ip_str) {
  net::IPAddress address;

  // Parse the string into an IPAddress object
  if (!net::ParseURLHostnameToAddress(raw_ip_str, &address)) {
    LOG(ERROR) << "Received IP address is not valid: " << raw_ip_str;
    return false;
  }

<<<<<<< HEAD
  struct in6_addr ipv6_addr;
  if (inet_pton(AF_INET6, ip_str.c_str(), &ipv6_addr) == 1) {
    // Check for the 2002::/16 prefix (6to4 address).
    // The first two bytes must be 0x20 and 0x02
    if (ipv6_addr.s6_addr[0] == 0x20 && ipv6_addr.s6_addr[1] == 0x02) {
      // Treat this address as IPv4 and convert to it.
      // Extract the 4 bytes following the prefix (indices 2, 3, 4, 5)
      uint32_t ipv4_network_order;
      std::memcpy(&ipv4_network_order, &ipv6_addr.s6_addr[2], sizeof(uint32_t));

      // Convert from Network Byte Order to Host Byte Order (uint32_t)
      uint32_t addr = ntohl(ipv4_network_order);
      return IsIPInPrivateRangeIPv4(addr);
    }

    // Unique Local Addresses for IPv6 are _effectively_ fd00::/8.
    // See https://tools.ietf.org/html/rfc4193#section-3 for details.
    //
    // RFC 4193: fc00::/7 (The first 7 bits must be 1111110)
    // This covers both fc00::/8 and fd00::/8
    // We check the first byte: (byte & 11111110) == 11111100
    return (ipv6_addr.s6_addr[0] & 0xFE) == 0xFC;
=======
  std::optional<net::IPAddress> addr_ipv4 = ExtractIPv4From6to4(address);
  if (addr_ipv4) {
    // This is potentially breaking convention since 6to4 addresses are
    // typically used as global unicast addresses routed by public relays.
    // However, due to Vega's testing implementations we need this exception to
    // allow such connections under the Cobalt CSP directive
    // "cobalt-insecure-private-range".
    LOG(WARNING) << "Treating 6to4 address as IPv4 for Private Range check.";
    address = addr_ipv4.value();
>>>>>>> 0147fa5bd7e (Refactor IP checks for custom cobalt CSP directive (#8549))
  }

  return network::IPAddressToIPAddressSpace(address) == network::mojom::IPAddressSpace::kPrivate;
}

}  // namespace blink

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/local_ip.h"

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "third_party/blink/public/platform/platform.h"

namespace blink {
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

bool IsIPInPrivateRange(const std::string& ip_str) {
  // For IPv4, the private address range is defined by RFC 1918
  // available at https://tools.ietf.org/html/rfc1918#section-3.
  struct in_addr ipv4_addr;
  if (inet_pton(AF_INET, ip_str.c_str(), &ipv4_addr) == 1) {
      // Convert to host byte order for easier comparison
      uint32_t addr = ntohl(ipv4_addr.s_addr);

      // 10.0.0.0/8
      if ((addr & 0xFF000000) == 0x0A000000) return true;

      // 172.16.0.0/12 (172.16.0.0 to 172.31.255.255)
      if ((addr & 0xFFF00000) == 0xAC100000) return true;

      // 192.168.0.0/16
      if ((addr & 0xFFFF0000) == 0xC0A80000) return true;

      return false;
  }

  // Unique Local Addresses for IPv6 are _effectively_ fd00::/8.
  // See https://tools.ietf.org/html/rfc4193#section-3 for details.
  struct in6_addr ipv6_addr;
  if (inet_pton(AF_INET6, ip_str.c_str(), &ipv6_addr) == 1) {
    // RFC 4193: fc00::/7 (The first 7 bits must be 1111110)
    // This covers both fc00::/8 and fd00::/8
    // We check the first byte: (byte & 11111110) == 11111100
    return (ipv6_addr.s6_addr[0] & 0xFE) == 0xFC;
  }

  // Not a valid IPv4 or IPv6 address
  return false;
}

}  // namespace blink

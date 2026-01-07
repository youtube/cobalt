// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/local_ip.h"

#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "base/notreached.h"
#include "net/base/ip_address.h"
#include "net/base/url_util.h"
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

bool IsIPInPrivateRange(const std::string& raw_ip_str) {
  net::IPAddress address;

  // Parse the string into an IPAddress object
  if (net::ParseURLHostnameToAddress(raw_ip_str, &address)) {
    LOG(ERROR) << "Received IP address is not valid: " << raw_ip_str;
    return false;
  }

  // Handle 6to4 or IPv4-mapped addresses
  // If it's an IPv6 address that encapsulates an IPv4 address,
  // we convert it to its IPv4 form to check RFC 1918 ranges correctly.
  if (address.IsIPv4MappedIPv6()) {
    address = net::ConvertIPv4MappedIPv6ToIPv4(address);
  }

  //  Note: we don't use IsReserved() since it includes loopback, link-local,
  //  multicast, and other special-use blocks.

  // For IPv4, the private address range is defined by RFC 1918
  // available at https://tools.ietf.org/html/rfc1918#section-3.
  if (address.IsIPv4()) {
    const net::IPAddress k10(10, 0, 0, 0);
    const net::IPAddress k172(172, 16, 0, 0);
    const net::IPAddress k192(192, 168, 0, 0);

    // 10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16
    return IPAddressMatchesPrefix(address, k10, 8) ||
           IPAddressMatchesPrefix(address, k172, 12) ||
           IPAddressMatchesPrefix(address, k192, 16);
  }

  // Unique Local Addresses for IPv6 are _effectively_ fd00::/8.
  // See https://tools.ietf.org/html/rfc4193#section-3 for details.
  if (address.IsIPv6()) {
    // RFC 4193: fc00::/7
    const uint8_t kUlaPrefix[] = {0xfc, 0};
    const net::IPAddress kUla(kUlaPrefix);
    return IPAddressMatchesPrefix(address, kUla, 7);
  }

  NOTREACHED();
  return false;
}

}  // namespace blink

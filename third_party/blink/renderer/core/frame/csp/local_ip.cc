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


  std::optional<net::IPAddress> addr_ipv4 = ExtractIPv4From6to4(address);
  if (addr_ipv4) {
    // This is potentially breaking convention since 6to4 addresses are
    // typically used as global unicast addresses routed by public relays.
    // However, due to Vega's testing implementations we need this exception to
    // allow such connections under the Cobalt CSP directive
    // "cobalt-insecure-private-range".
    LOG(WARNING) << "Treating 6to4 address as IPv4 for Private Range check.";
    address = addr_ipv4.value();
  }

  return network::IPAddressToIPAddressSpace(address) == mojom::IPAddressSpace::kLocal;
}

}  // namespace blink

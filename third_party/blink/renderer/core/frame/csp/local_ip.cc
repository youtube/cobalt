// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/local_ip.h"

#include <ifaddrs.h>
#include <arpa/inet.h>

#include "third_party/blink/public/platform/platform.h"

namespace blink {
// Queries host IP address and matches against netmask of target ip to determine.
bool IsIPInLocalNetwork(const std::string& target_ip_str) {
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

}  // namespace blink

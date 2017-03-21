// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "starboard/socket.h"

#if SB_API_VERSION < SB_SOCKET_GET_SOURCE_ADDRESS_AND_NETMASK_VERSION

#include <arpa/inet.h>
#include <ifaddrs.h>

#include "starboard/log.h"
#include "starboard/shared/posix/socket_internal.h"

namespace sbposix = starboard::shared::posix;

bool SbSocketGetLocalInterfaceAddress(SbSocketAddress* out_address) {
  if (!out_address) {
    return false;
  }

  // Get a list of the local network interfaces.
  struct ifaddrs* ifaddr;
  if (getifaddrs(&ifaddr) == -1) {
    return false;
  }

  // Scan the returned interfaces for a non-loopback IPv4 interface.
  for (struct ifaddrs* it = ifaddr; it != NULL; it = it->ifa_next) {
    if (it->ifa_addr == NULL) {
      continue;
    }

    // Filter out anything but IPv4.
    if (it->ifa_addr->sa_family != AF_INET) {
      continue;
    }

    // We know it is IPV4, so let's cast it over.
    struct sockaddr_in* if_addr =
        reinterpret_cast<struct sockaddr_in*>(it->ifa_addr);

    // Filter out the loopback adapter.
    if (if_addr->sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
      continue;
    }

    // Copy the address to the destination.
    sbposix::SockAddr sock_addr;
    sock_addr.FromSockaddr(it->ifa_addr);
    if (!sock_addr.ToSbSocketAddress(out_address)) {
      continue;
    }

    freeifaddrs(ifaddr);
    return true;
  }

  freeifaddrs(ifaddr);
  return false;
}

#endif  // SB_API_VERSION < SB_SOCKET_GET_SOURCE_ADDRESS_AND_NETMASK_VERSION

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

#include "starboard/socket.h"

// linux/if.h assumes the symbols for structs defined in ifaddrs.h are
// already present. These includes must be above <linux/if.h> below.
#include <arpa/inet.h>
#include <ifaddrs.h>

#if SB_HAS_QUIRK(SOCKET_BSD_HEADERS)
#include <errno.h>
#include <net/if.h>
#include <net/if_dl.h>
#else
#include <linux/if.h>
#include <linux/if_addr.h>
#include <netdb.h>
#endif

#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "starboard/byte_swap.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/posix/socket_internal.h"

namespace sbposix = starboard::shared::posix;

namespace {

// TODO: Move this constant to socket.h.
const int kIPv6AddressSize = 16;

bool IsAnyAddress(const SbSocketAddress& address) {
  switch (address.type) {
    case kSbSocketAddressTypeIpv4:
      return (address.address[0] == 0 && address.address[1] == 0 &&
              address.address[2] == 0 && address.address[3] == 0);
#if SB_HAS(IPV6)
    case kSbSocketAddressTypeIpv6: {
      bool found_nonzero = false;
      for (std::size_t i = 0; i != kIPv6AddressSize; ++i) {
        found_nonzero |= (address.address[i] != 0);
      }
      return !found_nonzero;
    }
#endif
    default:
      SB_NOTREACHED() << "Invalid address type " << address.type;
      break;
  }

  return false;
}

template <typename T, int source_size>
void CopyIntoObjectFromArray(T* out_destination,
                             const unsigned char(&source)[source_size]) {
  SB_COMPILE_ASSERT(sizeof(T) <= source_size, destination_is_too_small);
  SbMemoryCopy(out_destination, source, sizeof(T));
}

bool GetPotentialMatch(const sockaddr* input_addr,
                       const in_addr** out_interface_addr) {
  if (!input_addr || input_addr->sa_family != AF_INET) {
    *out_interface_addr = NULL;
    return false;
  }

  const sockaddr_in* v4input_addr =
      reinterpret_cast<const sockaddr_in*>(input_addr);
  *out_interface_addr = &(v4input_addr->sin_addr);
  return true;
}

bool GetPotentialMatch(const sockaddr* input_addr,
                       const in6_addr** out_interface_addr) {
  if (!input_addr || input_addr->sa_family != AF_INET6) {
    *out_interface_addr = NULL;
    return false;
  }

  const sockaddr_in6* v6input_addr =
      reinterpret_cast<const sockaddr_in6*>(input_addr);
  *out_interface_addr = &(v6input_addr->sin6_addr);
  return true;
}

template <typename in_addr_type>
bool GetNetmaskForInterfaceAddress(const SbSocketAddress& interface_address,
                                   SbSocketAddress* out_netmask) {
  SB_DCHECK((interface_address.type == kSbSocketAddressTypeIpv4) ||
            (interface_address.type == kSbSocketAddressTypeIpv6));
  struct ifaddrs* interface_addrs = NULL;

  int retval = getifaddrs(&interface_addrs);
  if (retval != 0) {
    return false;
  }

  in_addr_type to_match;
  CopyIntoObjectFromArray(&to_match, interface_address.address);

  bool found_netmask = false;
  for (struct ifaddrs* interface = interface_addrs; interface != NULL;
       interface = interface->ifa_next) {
    if (!(IFF_UP & interface->ifa_flags) ||
        (IFF_LOOPBACK & interface->ifa_flags)) {
      continue;
    }

    const in_addr_type* potential_match;
    if (!GetPotentialMatch(interface->ifa_addr, &potential_match))
      continue;

    if (SbMemoryCompare(&to_match, potential_match, sizeof(in_addr_type)) !=
        0) {
      continue;
    }

    sbposix::SockAddr sock_addr;
    sock_addr.FromSockaddr(interface->ifa_netmask);
    if (sock_addr.ToSbSocketAddress(out_netmask)) {
      found_netmask = true;
      break;
    }
  }

  freeifaddrs(interface_addrs);

  return found_netmask;
}

bool GetNetMaskForInterfaceAddress(const SbSocketAddress& interface_address,
                                   SbSocketAddress* out_netmask) {
  SB_DCHECK(out_netmask);

  switch (interface_address.type) {
    case kSbSocketAddressTypeIpv4:
      return GetNetmaskForInterfaceAddress<in_addr>(interface_address,
                                                    out_netmask);
#if SB_HAS(IPV6)
    case kSbSocketAddressTypeIpv6:
      return GetNetmaskForInterfaceAddress<in6_addr>(interface_address,
                                                     out_netmask);
#endif
    default:
      SB_NOTREACHED() << "Invalid address type " << interface_address.type;
      break;
  }

  return false;
}

bool FindIPv4InterfaceIP(SbSocketAddress* out_interface_ip,
                         SbSocketAddress* out_netmask) {
  if (out_interface_ip == NULL) {
    SB_NOTREACHED() << "out_interface_ip must be specified";
    return false;
  }
  struct ifaddrs* interface_addrs = NULL;

  int retval = getifaddrs(&interface_addrs);
  if (retval != 0) {
    return false;
  }

  bool success = false;
  for (struct ifaddrs* interface = interface_addrs; interface != NULL;
       interface = interface->ifa_next) {
    if (!(IFF_UP & interface->ifa_flags) ||
        (IFF_LOOPBACK & interface->ifa_flags)) {
      continue;
    }

    const struct sockaddr* addr = interface->ifa_addr;
    const struct sockaddr* netmask = interface->ifa_netmask;
    if (!addr || !netmask || (addr->sa_family != AF_INET)) {
      // IPv4 addresses only.
      continue;
    }

    sbposix::SockAddr sock_addr;
    sock_addr.FromSockaddr(addr);
    if (sock_addr.ToSbSocketAddress(out_interface_ip)) {
      if (out_netmask) {
        sbposix::SockAddr netmask_addr;
        netmask_addr.FromSockaddr(netmask);
        if (!netmask_addr.ToSbSocketAddress(out_netmask)) {
          continue;
        }
      }

      success = true;
      break;
    }
  }

  freeifaddrs(interface_addrs);

  return success;
}

#if SB_HAS(IPV6)
bool IsUniqueLocalAddress(const unsigned char ip[16]) {
  // Unique Local Addresses are in fd08::/8.
  return ip[0] == 0xfd && ip[1] == 0x08;
}

bool FindIPv6InterfaceIP(SbSocketAddress* out_interface_ip,
                         SbSocketAddress* out_netmask) {
  if (!out_interface_ip) {
    SB_NOTREACHED() << "out_interface_ip must be specified";
    return false;
  }
  struct ifaddrs* interface_addrs = NULL;

  int retval = getifaddrs(&interface_addrs);
  if (retval != 0) {
    return false;
  }

  int max_scope_interface_value = -1;

  bool ip_found = false;
  SbSocketAddress temp_interface_ip;
  SbSocketAddress temp_netmask;

  for (struct ifaddrs* interface = interface_addrs; interface != NULL;
       interface = interface->ifa_next) {
    if (!(IFF_UP & interface->ifa_flags) ||
        (IFF_LOOPBACK & interface->ifa_flags)) {
      continue;
    }

    const struct sockaddr* addr = interface->ifa_addr;
    const struct sockaddr* netmask = interface->ifa_netmask;
    if (!addr || !netmask || addr->sa_family != AF_INET6) {
      // IPv6 addresses only.
      continue;
    }

    const in6_addr* potential_match;
    if (!GetPotentialMatch(interface->ifa_addr, &potential_match))
      continue;

    // Check the IP for loopback again, just in case flags were incorrect.
    if (IN6_IS_ADDR_LOOPBACK(potential_match) ||
        IN6_IS_ADDR_LINKLOCAL(potential_match)) {
      continue;
    }

    const sockaddr_in6* v6addr =
        reinterpret_cast<const sockaddr_in6*>(interface->ifa_addr);
    if (!v6addr) {
      continue;
    }

    int current_interface_scope = v6addr->sin6_scope_id;

    if (IsUniqueLocalAddress(v6addr->sin6_addr.s6_addr)) {
      // ULAs have global scope, but not globally routable.  So prefer
      // non ULA addresses with global scope by adjusting their "scope"
      current_interface_scope -= 1;
    }

    if (current_interface_scope <= max_scope_interface_value) {
      continue;
    }
    max_scope_interface_value = current_interface_scope;

    sbposix::SockAddr sock_addr;
    sock_addr.FromSockaddr(addr);
    if (sock_addr.ToSbSocketAddress(&temp_interface_ip)) {
      if (netmask) {
        sbposix::SockAddr netmask_addr;
        netmask_addr.FromSockaddr(netmask);
        if (!netmask_addr.ToSbSocketAddress(&temp_netmask)) {
          continue;
        }
      }

      ip_found = true;
    }
  }

  freeifaddrs(interface_addrs);

  if (!ip_found) {
    return false;
  }

  SbMemoryCopy(out_interface_ip, &temp_interface_ip, sizeof(SbSocketAddress));
  if (out_netmask != NULL) {
    SbMemoryCopy(out_netmask, &temp_netmask, sizeof(SbSocketAddress));
  }

  return true;
}
#endif

bool FindInterfaceIP(const SbSocketAddressType type,
                     SbSocketAddress* out_interface_ip,
                     SbSocketAddress* out_netmask) {
  switch (type) {
    case kSbSocketAddressTypeIpv4:
      return FindIPv4InterfaceIP(out_interface_ip, out_netmask);
#if SB_HAS(IPV6)
    case kSbSocketAddressTypeIpv6:
      return FindIPv6InterfaceIP(out_interface_ip, out_netmask);
#endif
    default:
      SB_NOTREACHED() << "Invalid socket address type " << type;
  }

  return false;
}

bool FindSourceAddressForDestination(const SbSocketAddress& destination,
                                     SbSocketAddress* out_source_address) {
  SbSocket socket = SbSocketCreate(destination.type, kSbSocketProtocolUdp);
  if (!SbSocketIsValid(socket)) {
    return false;
  }

  SbSocketError connect_retval = SbSocketConnect(socket, &destination);
  if (connect_retval != kSbSocketOk) {
    bool socket_destroyed = SbSocketDestroy(socket);
    SB_DCHECK(socket_destroyed);
    return false;
  }

  bool success = SbSocketGetLocalAddress(socket, out_source_address);
  bool socket_destroyed = SbSocketDestroy(socket);
  SB_DCHECK(socket_destroyed);
  return success;
}

}  // namespace

bool SbSocketGetInterfaceAddress(const SbSocketAddress* const destination,
                                 SbSocketAddress* out_source_address,
                                 SbSocketAddress* out_netmask) {
  if (!out_source_address) {
    return false;
  }

  if (destination == NULL) {
#if SB_HAS(IPV6)
    // Return either a v4 or a v6 address.  Per spec.
    return (FindIPv4InterfaceIP(out_source_address, out_netmask) ||
            FindIPv6InterfaceIP(out_source_address, out_netmask));
#else
    return FindIPv4InterfaceIP(out_source_address, out_netmask);
#endif

  } else if (IsAnyAddress(*destination)) {
    return FindInterfaceIP(destination->type, out_source_address, out_netmask);
  } else {
    SbSocketAddress destination_copy = *destination;
    // On some platforms, passing a socket address with port 0 to connect()
    // results in EADDRNOTAVAIL.
    if (!destination_copy.port) {
      destination_copy.port = 80;
    }
    return (
        FindSourceAddressForDestination(destination_copy, out_source_address) &&
        GetNetMaskForInterfaceAddress(*out_source_address, out_netmask));
  }

  return false;
}

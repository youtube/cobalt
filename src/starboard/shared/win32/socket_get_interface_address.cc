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

#include <winsock2.h>

#include <ifdef.h>
#include <iphlpapi.h>

#include <algorithm>
#include <memory>

#include "starboard/byte_swap.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/win32/socket_internal.h"

namespace sbwin32 = starboard::shared::win32;

namespace {
const ULONG kDefaultAdapterInfoBufferSizeInBytes = 16 * 1024;

bool IsAnyAddress(const SbSocketAddress& address) {
  switch (address.type) {
    case kSbSocketAddressTypeIpv4:
      return (address.address[0] == 0 && address.address[1] == 0 &&
              address.address[2] == 0 && address.address[3] == 0);
    case kSbSocketAddressTypeIpv6: {
      bool found_nonzero = false;
      for (std::size_t i = 0; i != sbwin32::kAddressLengthIpv6; ++i) {
        found_nonzero |= (address.address[i] != 0);
      }
      return !found_nonzero;
    }
    default:
      SB_NOTREACHED() << "Invalid address type " << address.type;
      break;
  }

  return false;
}

void GenerateNetMaskFromPrefixLength(UINT8 prefix_length,
                                     UINT32* const address_begin,
                                     UINT32* const address_end) {
  SB_DCHECK(address_end >= address_begin);
  SB_DCHECK((reinterpret_cast<char*>(address_end) -
             reinterpret_cast<char*>(address_begin)) %
                4 ==
            0);
  UINT8 ones_left = prefix_length;
  const int kBitsInOneDWORD = sizeof(UINT32) * 8;
  for (UINT32* iterator = address_begin; iterator != address_end; ++iterator) {
    UINT8 ones_in_this_dword = std::min<UINT8>(kBitsInOneDWORD, ones_left);
    UINT64 mask_value =
        kSbUInt64Max - ((1ULL << (kBitsInOneDWORD - ones_in_this_dword)) - 1);
    *iterator =
        SB_HOST_TO_NET_U32(static_cast<UINT32>(mask_value & kSbUInt64Max));
    ones_left -= ones_in_this_dword;
  }
}

bool PopulateInterfaceAddress(const IP_ADAPTER_UNICAST_ADDRESS& unicast_address,
                              SbSocketAddress* out_interface_ip) {
  if (!out_interface_ip) {
    return true;
  }

  const SOCKET_ADDRESS& address = unicast_address.Address;
  sbwin32::SockAddr addr;
  return addr.FromSockaddr(address.lpSockaddr) &&
         addr.ToSbSocketAddress(out_interface_ip);
}

bool PopulateNetmask(const IP_ADAPTER_UNICAST_ADDRESS& unicast_address,
                     SbSocketAddress* out_netmask) {
  if (!out_netmask) {
    return true;
  }

  const SOCKET_ADDRESS& address = unicast_address.Address;
  if (address.lpSockaddr == nullptr) {
    return false;
  }
  const ADDRESS_FAMILY& family = address.lpSockaddr->sa_family;

  switch (family) {
    case AF_INET:
      out_netmask->type = kSbSocketAddressTypeIpv4;
      break;
    case AF_INET6:
      out_netmask->type = kSbSocketAddressTypeIpv6;
      break;
    default:
      SB_NOTREACHED() << "Invalid family " << family;
      return false;
  }

  UINT32* const begin_netmask =
      reinterpret_cast<UINT32*>(&(out_netmask->address[0]));
  UINT32* const end_netmask =
      begin_netmask + SB_ARRAY_SIZE(out_netmask->address) / sizeof(UINT32);

  GenerateNetMaskFromPrefixLength(unicast_address.OnLinkPrefixLength,
                                  begin_netmask, end_netmask);
  return true;
}

bool GetAdapters(const SbSocketAddressType address_type,
                 std::unique_ptr<char[]>* adapter_info) {
  SB_DCHECK(adapter_info);

  ULONG family = 0;
  int address_length_bytes = 0;

  switch (address_type) {
    case kSbSocketAddressTypeIpv4:
      family = AF_INET;
      address_length_bytes = sbwin32::kAddressLengthIpv4;
      break;
    case kSbSocketAddressTypeIpv6:
      family = AF_INET6;
      address_length_bytes = sbwin32::kAddressLengthIpv6;
      break;
    default:
      SB_NOTREACHED() << "Invalid address type: " << address_type;
      return false;
  }

  ULONG adapter_addresses_number_bytes = kDefaultAdapterInfoBufferSizeInBytes;

  for (int try_count = 0; try_count != 2; ++try_count) {
    // Using auto for return value here, since different versions of windows use
    // slightly different datatypes.  These differences do not matter to us, but
    // the compiler might warn on them.
    adapter_info->reset(new char[adapter_addresses_number_bytes]);
    PIP_ADAPTER_ADDRESSES adapter_addresses =
        reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapter_info->get());

    // Note: If |GetAdapterAddresses| deems that buffer supplied is not enough,
    // it will return the recommended number of bytes in
    // |adapter_addresses_number_bytes|.
    auto retval = GetAdaptersAddresses(
        family, GAA_FLAG_SKIP_FRIENDLY_NAME | GAA_FLAG_SKIP_DNS_SERVER, nullptr,
        adapter_addresses, &adapter_addresses_number_bytes);

    if (retval == ERROR_SUCCESS) {
      return true;
    }
    if (retval != ERROR_BUFFER_OVERFLOW) {
      // Only retry with more memory if the error says so.
      break;
    }
    SB_LOG(ERROR) << "GetAdapterAddresses() failed with error code " << retval;
  }

  return false;
}

bool GetNetmaskForInterfaceAddress(const SbSocketAddress& interface_address,
                                   SbSocketAddress* out_netmask) {
  std::unique_ptr<char[]> adapter_info_memory_block;
  if (!GetAdapters(interface_address.type, &adapter_info_memory_block)) {
    return false;
  }
  const void* const interface_address_buffer =
      reinterpret_cast<const void* const>(interface_address.address);
  for (PIP_ADAPTER_ADDRESSES adapter = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(
           adapter_info_memory_block.get());
       adapter != nullptr; adapter = adapter->Next) {
    if ((adapter->OperStatus != IfOperStatusUp) ||
        (adapter->IfType != IF_TYPE_ETHERNET_CSMACD)) {
      // Note: IfType == IF_TYPE_SOFTWARE_LOOPBACK is also skipped.
      continue;
    }

    for (PIP_ADAPTER_UNICAST_ADDRESS unicast_address =
             adapter->FirstUnicastAddress;
         unicast_address != nullptr; unicast_address = unicast_address->Next) {
      sbwin32::SockAddr addr;
      if (!addr.FromSockaddr(unicast_address->Address.lpSockaddr)) {
        continue;
      }

      const void* unicast_address_buffer = nullptr;
      int bytes_to_check = 0;

      switch (interface_address.type) {
        case kSbSocketAddressTypeIpv4:
          unicast_address_buffer =
              reinterpret_cast<void*>(&(addr.sockaddr_in()->sin_addr));
          bytes_to_check = sbwin32::kAddressLengthIpv4;
          break;
        case kSbSocketAddressTypeIpv6:
          unicast_address_buffer =
              reinterpret_cast<void*>(&(addr.sockaddr_in6()->sin6_addr));
          bytes_to_check = sbwin32::kAddressLengthIpv6;
          break;
        default:
          SB_DLOG(ERROR) << "Invalid interface address type "
                         << interface_address.type;
          return false;
      }

      if (SbMemoryCompare(unicast_address_buffer, interface_address_buffer,
                          bytes_to_check) != 0) {
        continue;
      }

      if (PopulateNetmask(*unicast_address, out_netmask)) {
        return true;
      }
    }
  }

  return false;
}

bool IsUniqueLocalAddress(const unsigned char ip[16]) {
  // Unique Local Addresses are in fd08::/8.
  return ip[0] == 0xfd && ip[1] == 0x08;
}

bool FindInterfaceIP(const SbSocketAddressType address_type,
                     SbSocketAddress* out_interface_ip,
                     SbSocketAddress* out_netmask) {
  if (out_interface_ip == nullptr) {
    SB_NOTREACHED() << "out_interface_ip must be specified";
    return false;
  }

  std::unique_ptr<char[]> adapter_info_memory_block;
  if (!GetAdapters(address_type, &adapter_info_memory_block)) {
    return false;
  }

  for (PIP_ADAPTER_ADDRESSES adapter = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(
           adapter_info_memory_block.get());
       adapter != nullptr; adapter = adapter->Next) {
    if ((adapter->OperStatus != IfOperStatusUp) ||
        (adapter->IfType != IF_TYPE_ETHERNET_CSMACD)) {
      // Note: IfType == IF_TYPE_SOFTWARE_LOOPBACK is also skipped.
      continue;
    }

    for (PIP_ADAPTER_UNICAST_ADDRESS unicast_address =
             adapter->FirstUnicastAddress;
         unicast_address != nullptr; unicast_address = unicast_address->Next) {
      if (unicast_address->Flags & (IP_ADAPTER_ADDRESS_TRANSIENT)) {
        continue;
      }
      if (!(unicast_address->Flags & IP_ADAPTER_ADDRESS_DNS_ELIGIBLE)) {
        continue;
      }

      // TODO: For IPv6, Prioritize interface with highest scope.
      // Skip ULAs for now.
      if (address_type == kSbSocketAddressTypeIpv6) {
        // Documentation on MSDN states:
        // "The SOCKADDR structure pointed to by the lpSockaddr member varies
        // depending on the protocol or address family selected. For example,
        // the sockaddr_in6 structure is used for an IPv6 socket address
        // while the sockaddr_in4 structure is used for an IPv4 socket address."
        // https://msdn.microsoft.com/en-us/library/windows/desktop/ms740507(v=vs.85).aspx

        sockaddr_in6* addr = reinterpret_cast<sockaddr_in6*>(
            unicast_address->Address.lpSockaddr);
        SB_DCHECK(addr->sin6_family == AF_INET6);
        if (IsUniqueLocalAddress(addr->sin6_addr.u.Byte)) {
          continue;
        }
      }

      if (!PopulateInterfaceAddress(*unicast_address, out_interface_ip)) {
        continue;
      }
      if (!PopulateNetmask(*unicast_address, out_netmask)) {
        continue;
      }

      return true;
    }
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

  if (destination == nullptr) {
    // Return either a v4 or a v6 address.  Per spec.
    return (FindInterfaceIP(kSbSocketAddressTypeIpv4, out_source_address,
                            out_netmask) ||
            FindInterfaceIP(kSbSocketAddressTypeIpv6, out_source_address,
                            out_netmask));
  } else if (IsAnyAddress(*destination)) {
    return FindInterfaceIP(destination->type, out_source_address, out_netmask);
  }

  return (FindSourceAddressForDestination(*destination, out_source_address) &&
          GetNetmaskForInterfaceAddress(*out_source_address, out_netmask));
}

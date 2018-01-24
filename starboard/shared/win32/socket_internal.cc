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

#include "starboard/shared/win32/socket_internal.h"

#include <winsock2.h>

#include "starboard/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace shared {
namespace win32 {

namespace {
const socklen_t kAddressStructLengthIpv4 =
    static_cast<socklen_t>(sizeof(struct sockaddr_in));
const socklen_t kAddressStructLengthIpv6 =
    static_cast<socklen_t>(sizeof(struct sockaddr_in6));
}

SbSocketError TranslateSocketErrorStatus(int error) {
  switch (error) {
    case 0:
      return kSbSocketOk;

    // Microsoft Winsock error codes:
    //   https://msdn.microsoft.com/en-us/library/windows/desktop/ms740668(v=vs.85).aspx
    case WSAEINPROGRESS:
    case WSAEWOULDBLOCK:
      return kSbSocketPending;
#if SB_HAS(SOCKET_ERROR_CONNECTION_RESET_SUPPORT) || \
    SB_API_VERSION >= 9
    case WSAECONNRESET:
    case WSAENETRESET:
      return kSbSocketErrorConnectionReset;

    // Microsoft System Error codes:
    //   https://msdn.microsoft.com/en-us/library/windows/desktop/ms681382(v=vs.85).aspx
    case ERROR_BROKEN_PIPE:
      return kSbSocketErrorConnectionReset;
#endif  // #if SB_HAS(SOCKET_ERROR_CONNECTION_RESET_SUPPORT) ||
        //     SB_API_VERSION >= 9
  }

  // Here's where we would be more nuanced if we need to be.
  return kSbSocketErrorFailed;
}

bool SetBooleanSocketOption(SbSocket socket,
                            int level,
                            int option_code,
                            const char* option_name,
                            bool value) {
  if (!SbSocketIsValid(socket)) {
    return false;
  }

  SB_DCHECK(socket->socket_handle != INVALID_SOCKET);
  const int on = value ? 1 : 0;
  int result = setsockopt(socket->socket_handle, level, option_code,
                          reinterpret_cast<const char*>(&on), sizeof(on));
  if (result == SOCKET_ERROR) {
    int last_error = WSAGetLastError();
    SB_DLOG(ERROR) << "Failed to set " << option_name << " on socket "
                   << socket->socket_handle << ", last_error = " << last_error;
    socket->error = TranslateSocketErrorStatus(last_error);
    return false;
  }

  socket->error = kSbSocketOk;
  return true;
}

bool SetIntegerSocketOption(SbSocket socket,
                            int level,
                            int option_code,
                            const char* option_name,
                            int value) {
  if (!SbSocketIsValid(socket)) {
    return false;
  }

  SB_DCHECK(socket->socket_handle != INVALID_SOCKET);
  int result = setsockopt(socket->socket_handle, level, option_code,
                          reinterpret_cast<const char*>(&value), sizeof(value));
  if (result == SOCKET_ERROR) {
    int last_error = WSAGetLastError();
    SB_DLOG(ERROR) << "Failed to set " << option_name << " on socket "
                   << socket->socket_handle << ", last_error = " << last_error;
    socket->error = TranslateSocketErrorStatus(last_error);
    return false;
  }

  socket->error = kSbSocketOk;
  return true;
}

bool SockAddr::FromSbSocketAddress(const SbSocketAddress* address) {
  if (!address) {
    return false;
  }

  length = sizeof(storage_);
  switch (address->type) {
    case kSbSocketAddressTypeIpv4: {
      struct sockaddr_in* addr = sockaddr_in();
      length = kAddressStructLengthIpv4;
      SbMemorySet(addr, 0, length);
      addr->sin_family = AF_INET;
      addr->sin_port = htons(static_cast<USHORT>(address->port));
      SbMemoryCopy(&addr->sin_addr, address->address, kAddressLengthIpv4);
      break;
    }
    case kSbSocketAddressTypeIpv6: {
      struct sockaddr_in6* addr6 = sockaddr_in6();
      length = kAddressStructLengthIpv6;
      SbMemorySet(addr6, 0, length);
      addr6->sin6_family = AF_INET6;
      addr6->sin6_port = htons(static_cast<USHORT>(address->port));
      SbMemoryCopy(&addr6->sin6_addr, address->address, kAddressLengthIpv6);
      break;
    }
    default:
      SB_NOTREACHED() << "Unrecognized address type: " << address->type;
      return false;
  }

  return true;
}

bool SockAddr::ToSbSocketAddress(SbSocketAddress* out_address) const {
  if (!out_address) {
    return false;
  }

// Check that we have been properly initialized.
  SB_DCHECK(length == kAddressStructLengthIpv4 ||
            length == kAddressStructLengthIpv6);

  if (family() == AF_INET) {
    const struct sockaddr_in* addr = sockaddr_in();
    if (length < kAddressStructLengthIpv4) {
      SB_NOTREACHED() << "Insufficient INET size: " << length;
      return false;
    }

    SbMemoryCopy(out_address->address, &addr->sin_addr, kAddressLengthIpv4);
    out_address->port = ntohs(addr->sin_port);
    out_address->type = kSbSocketAddressTypeIpv4;
    return true;
  }

  if (family() == AF_INET6) {
    const struct sockaddr_in6* addr6 = sockaddr_in6();
    if (length < kAddressStructLengthIpv6) {
      SB_NOTREACHED() << "Insufficient INET6 size: " << length;
      return false;
    }

    SbMemoryCopy(out_address->address, &addr6->sin6_addr, kAddressLengthIpv6);
    out_address->port = ntohs(addr6->sin6_port);
    out_address->type = kSbSocketAddressTypeIpv6;
    return true;
  }

  SB_NOTREACHED() << "Unrecognized address family: " << family();
  return false;
}

bool SockAddr::FromSockaddr(const struct sockaddr* sock_addr) {
  if (!sock_addr) {
    return false;
  }

  int family = sock_addr->sa_family;
  if (family == AF_INET) {
    const struct sockaddr_in* addr =
        reinterpret_cast<const struct sockaddr_in*>(sock_addr);
    *sockaddr_in() = *addr;
    length = static_cast<socklen_t>(sizeof(*addr));
    return true;
  } else if (family == AF_INET6) {
    const struct sockaddr_in6* addr =
        reinterpret_cast<const struct sockaddr_in6*>(sock_addr);
    *sockaddr_in6() = *addr;
    length = static_cast<socklen_t>(sizeof(*addr));
    return true;
  }

  SB_DLOG(WARNING) << "Unrecognized address family: " << family;
  return false;
}

}  // namespace win32
}  // namespace shared
}  // namespace starboard

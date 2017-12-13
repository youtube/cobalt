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

#include "starboard/shared/posix/socket_internal.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "starboard/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace shared {
namespace posix {

namespace {
const socklen_t kAddressLengthIpv4 = 4;
const socklen_t kAddressStructLengthIpv4 =
    static_cast<socklen_t>(sizeof(struct sockaddr_in));
#if SB_HAS(IPV6)
const socklen_t kAddressLengthIpv6 = 16;
const socklen_t kAddressStructLengthIpv6 =
    static_cast<socklen_t>(sizeof(struct sockaddr_in6));
#endif
}

SbSocketError TranslateSocketErrno(int error) {
  switch (error) {
    case 0:
      return kSbSocketOk;
    case EINPROGRESS:
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
      return kSbSocketPending;
#if SB_API_VERSION >= SB_ADDITIONAL_SOCKET_CONNECTION_ERRORS_API_VERSION
    case ECONNRESET:
    case ENETRESET:
    case EPIPE:
      return kSbSocketErrorConnectionReset;
#endif  // #if SB_API_VERSION >=
        // SB_ADDITIONAL_SOCKET_CONNECTION_ERRORS_API_VERSION
  }

  SB_LOG(ERROR) << "ERROR: " << error;

  // Here's where we would be more nuanced if we need to be.
  return kSbSocketErrorFailed;
}

bool SetBooleanSocketOption(SbSocket socket,
                            int level,
                            int option_code,
                            const char* option_name,
                            bool value) {
  if (!SbSocketIsValid(socket)) {
    errno = EBADF;
    return false;
  }

  SB_DCHECK(socket->socket_fd >= 0);
  int on = value ? 1 : 0;
  int result =
      setsockopt(socket->socket_fd, level, option_code, &on, sizeof(on));
  if (result != 0) {
    SB_DLOG(ERROR) << "Failed to set " << option_name << " on socket "
                   << socket->socket_fd << ", errno = " << errno;
    socket->error = TranslateSocketErrno(errno);
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
    errno = EBADF;
    return false;
  }

  SB_DCHECK(socket->socket_fd >= 0);
  int result =
      setsockopt(socket->socket_fd, level, option_code, &value, sizeof(value));
  if (result != 0) {
    SB_DLOG(ERROR) << "Failed to set " << option_name << " on socket "
                   << socket->socket_fd << ", errno = " << errno;
    socket->error = TranslateSocketErrno(errno);
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
      addr->sin_port = htons(address->port);
      SbMemoryCopy(&addr->sin_addr, address->address, kAddressLengthIpv4);
      break;
    }
#if SB_HAS(IPV6)
    case kSbSocketAddressTypeIpv6: {
      struct sockaddr_in6* addr6 = sockaddr_in6();
      length = kAddressStructLengthIpv6;
      SbMemorySet(addr6, 0, length);
      addr6->sin6_family = AF_INET6;
      addr6->sin6_port = htons(address->port);
      SbMemoryCopy(&addr6->sin6_addr, address->address, kAddressLengthIpv6);
      break;
    }
#endif
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
#if SB_HAS(IPV6)
  SB_DCHECK(length == kAddressStructLengthIpv4 ||
            length == kAddressStructLengthIpv6);
#else
  SB_DCHECK(length == kAddressStructLengthIpv4);
#endif

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

#if SB_HAS(IPV6)
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
#endif

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
#if SB_HAS(IPV6)
  } else if (family == AF_INET6) {
    const struct sockaddr_in6* addr =
        reinterpret_cast<const struct sockaddr_in6*>(sock_addr);
    *sockaddr_in6() = *addr;
    length = static_cast<socklen_t>(sizeof(*addr));
    return true;
#endif
  }

  SB_DLOG(WARNING) << "Unrecognized address family: " << family;
  return false;
}

}  // namespace posix
}  // namespace shared
}  // namespace starboard

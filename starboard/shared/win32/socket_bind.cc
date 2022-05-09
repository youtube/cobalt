// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/socket.h"

#include <winsock2.h>

#include "starboard/common/log.h"
#include "starboard/common/memory.h"
#include "starboard/shared/win32/socket_internal.h"

namespace sbwin32 = starboard::shared::win32;

namespace {

bool IsIpv6InaddrAny(const SbSocketAddress* local_address) {
  return starboard::common::MemoryIsZero(local_address->address,
                                         sbwin32::kAddressLengthIpv6);
}
bool IsIpv4InaddrAny(const SbSocketAddress* local_address) {
  return starboard::common::MemoryIsZero(local_address->address,
                                         sbwin32::kAddressLengthIpv4);
}

}  // namespace

SbSocketError SbSocketBind(SbSocket socket,
                           const SbSocketAddress* local_address) {
  if (!SbSocketIsValid(socket)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid socket";
    return kSbSocketErrorFailed;
  }

  sbwin32::SockAddr sock_addr;
  if (!sock_addr.FromSbSocketAddress(local_address)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid address";
    return (socket->error = sbwin32::TranslateSocketErrorStatus(EINVAL));
  }

  SB_DCHECK(socket->socket_handle != INVALID_SOCKET);
  if (local_address->type != socket->address_type) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Incompatible addresses: "
                   << "socket type = " << socket->address_type
                   << ", argument type = " << local_address->type;
    return (socket->error = sbwin32::TranslateSocketErrorStatus(EAFNOSUPPORT));
  }

  switch (local_address->type) {
    case kSbSocketAddressTypeIpv6:
      if (!IsIpv6InaddrAny(local_address)) {
        socket->bound_to = SbSocketPrivate::BindTarget::kOther;
        break;
      }

      socket->bound_to = SbSocketPrivate::BindTarget::kAny;

      // When binding to the IPV6 any address, ensure that the IPV6_V6ONLY flag
      // is off to allow incoming IPV4 connections on the same socket.
      // See https://www.ietf.org/rfc/rfc3493.txt for details.
      if (!sbwin32::SetBooleanSocketOption(socket, IPPROTO_IPV6, IPV6_V6ONLY,
                                           "IPV6_V6ONLY", false)) {
        // Silently ignore errors, assume the default behavior is as expected.
        socket->error = kSbSocketOk;
      }

      break;
    case kSbSocketAddressTypeIpv4:
      socket->bound_to = IsIpv4InaddrAny(local_address)
                             ? SbSocketPrivate::BindTarget::kAny
                             : SbSocketPrivate::BindTarget::kOther;
      break;
  }

  int result =
      bind(socket->socket_handle, sock_addr.sockaddr(), sock_addr.length);
  if (result == SOCKET_ERROR) {
    int last_error = WSAGetLastError();
    SB_DLOG(ERROR) << __FUNCTION__
                   << ": Bind failed. last_error=" << last_error;
    return (socket->error = sbwin32::TranslateSocketErrorStatus(last_error));
  }

  return (socket->error = kSbSocketOk);
}

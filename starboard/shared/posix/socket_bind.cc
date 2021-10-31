// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <errno.h>
#include <sys/socket.h>

#include "starboard/common/log.h"
#include "starboard/common/memory.h"
#include "starboard/shared/posix/handle_eintr.h"
#include "starboard/shared/posix/socket_internal.h"

namespace sbposix = starboard::shared::posix;

SbSocketError SbSocketBind(SbSocket socket,
                           const SbSocketAddress* local_address) {
  if (!SbSocketIsValid(socket)) {
    errno = EBADF;
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid socket";
    return kSbSocketErrorFailed;
  }

  sbposix::SockAddr sock_addr;
  if (!sock_addr.FromSbSocketAddress(local_address)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid address";
    return (socket->error = sbposix::TranslateSocketErrno(EINVAL));
  }

  SB_DCHECK(socket->socket_fd >= 0);
  if (local_address->type != socket->address_type) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Incompatible addresses: "
                   << "socket type = " << socket->address_type
                   << ", argument type = " << local_address->type;
    return (socket->error = sbposix::TranslateSocketErrno(EAFNOSUPPORT));
  }

#if SB_HAS(IPV6)
  // When binding to the IPV6 any address, ensure that the IPV6_V6ONLY flag is
  // off to allow incoming IPV4 connections on the same socket.
  // See https://www.ietf.org/rfc/rfc3493.txt for details.
  if (local_address && (local_address->type == kSbSocketAddressTypeIpv6) &&
      starboard::common::MemoryIsZero(local_address->address, 16)) {
    if (!sbposix::SetBooleanSocketOption(socket, IPPROTO_IPV6, IPV6_V6ONLY,
                                         "IPV6_V6ONLY", false)) {
      // Silently ignore errors, assume the default behavior is as expected.
      socket->error = kSbSocketOk;
    }
  }
#endif

  int result = HANDLE_EINTR(
      bind(socket->socket_fd, sock_addr.sockaddr(), sock_addr.length));
  if (result != 0) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Bind failed. errno=" << errno;
    return (socket->error = sbposix::TranslateSocketErrno(errno));
  }

  return (socket->error = kSbSocketOk);
}

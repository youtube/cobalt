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

#include "starboard/log.h"
#include "starboard/shared/win32/socket_internal.h"

namespace sbwin32 = starboard::shared::win32;

SbSocketError SbSocketConnect(SbSocket socket, const SbSocketAddress* address) {
  if (!SbSocketIsValid(socket)) {
    return kSbSocketErrorFailed;
  }

  sbwin32::SockAddr sock_addr;
  if (!sock_addr.FromSbSocketAddress(address)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid address";
    return (socket->error = kSbSocketErrorFailed);
  }

  SB_DCHECK(socket->socket_handle != INVALID_SOCKET);
  if (address->type != socket->address_type) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Incompatible addresses: "
                   << "socket type = " << socket->address_type
                   << ", argument type = " << address->type;
    return (socket->error = kSbSocketErrorFailed);
  }

  int result =
      connect(socket->socket_handle, sock_addr.sockaddr(), sock_addr.length);

  if (result != SOCKET_ERROR) {
    socket->bound_to = SbSocketPrivate::BindTarget::kAny;
    return (socket->error = kSbSocketOk);
  }

  const int last_error = WSAGetLastError();
  if (last_error == WSAEWOULDBLOCK) {
    socket->bound_to = SbSocketPrivate::BindTarget::kAny;
    return (socket->error = kSbSocketPending);
  }

  SB_DLOG(ERROR) << __FUNCTION__ << ": connect failed: " << last_error;
  return (socket->error = kSbSocketErrorFailed);
}

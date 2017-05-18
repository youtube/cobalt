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

SbSocketError SbSocketListen(SbSocket socket) {
  if (!SbSocketIsValid(socket)) {
    return kSbSocketErrorFailed;
  }

  SB_DCHECK(socket->socket_handle != INVALID_SOCKET);

  // Under winsock, a socket must be bound before we can listen on it.
  if (socket->bound_to == SbSocketPrivate::BindTarget::kUnbound) {
    // By listening on ::, we can accept both IPv4 and IPv6 connections.
    SbSocketAddress any_address = {0};
    any_address.type = socket->address_type;
    if (SbSocketBind(socket, &any_address) != kSbSocketOk) {
      SB_DLOG(ERROR) << "Unable to bind a socket during SbListen.";
      return (socket->error = kSbSocketErrorFailed);
    }

    socket->bound_to = SbSocketPrivate::BindTarget::kAny;
  }

  // TODO: Determine if we need to specify a > 0 backlog. It can go up to
  // SOMAXCONN according to the documentation. Several places in chromium
  // specify the literal "10" with the comment "maybe dont allow any backlog?"
  int result = listen(socket->socket_handle, 0);
  if (result == SOCKET_ERROR) {
    int last_error = WSAGetLastError();
    SB_LOG(ERROR) << "listen() failed with last_error = " << last_error;
    return (socket->error = sbwin32::TranslateSocketErrorStatus(last_error));
  }

  return (socket->error = kSbSocketOk);
}

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
#include "starboard/shared/win32/set_non_blocking_internal.h"
#include "starboard/shared/win32/socket_internal.h"

namespace sbwin32 = starboard::shared::win32;

SbSocket SbSocketAccept(SbSocket socket) {
  if (!SbSocketIsValid(socket)) {
    return kSbSocketInvalid;
  }

  SB_DCHECK(socket->socket_handle != INVALID_SOCKET);

  SOCKET socket_handle = accept(socket->socket_handle, nullptr, nullptr);
  if (socket_handle == INVALID_SOCKET) {
    socket->error = sbwin32::TranslateSocketErrorStatus(WSAGetLastError());
    return kSbSocketInvalid;
  }

  // All Starboard sockets are non-blocking, so let's ensure it.
  if (!sbwin32::SetNonBlocking(socket_handle)) {
    // Something went wrong, we'll clean up and return failure.
    socket->error = sbwin32::TranslateSocketErrorStatus(WSAGetLastError());
    closesocket(socket_handle);
    return kSbSocketInvalid;
  }

  socket->error = kSbSocketOk;

  // Adopt the newly accepted socket.
  return new SbSocketPrivate(socket->address_type, socket->protocol,
                             socket_handle,
                             SbSocketPrivate::BindTarget::kAccepted);
}

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
#include "starboard/shared/win32/socket_internal.h"

namespace sbwin32 = starboard::shared::win32;

bool SbSocketJoinMulticastGroup(SbSocket socket,
                                const SbSocketAddress* address) {
  if (!SbSocketIsValid(socket)) {
    return false;
  }

  if (!address) {
    return false;
  }

  SB_DCHECK(socket->socket_handle != INVALID_SOCKET);
  ip_mreq imreq = {0};
  IN_ADDR addr = *reinterpret_cast<const IN_ADDR*>(address->address);
  imreq.imr_multiaddr.s_addr = addr.S_un.S_addr;
  imreq.imr_interface.s_addr = INADDR_ANY;

  int result = setsockopt(socket->socket_handle, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                          reinterpret_cast<const char*>(&imreq), sizeof(imreq));
  if (result == SOCKET_ERROR) {
    int last_error = WSAGetLastError();
    SB_DLOG(ERROR) << "Failed to IP_ADD_MEMBERSHIP on socket "
                   << socket->socket_handle << ", last_error = " << last_error;
    socket->error = sbwin32::TranslateSocketErrorStatus(last_error);
    return false;
  }

  socket->error = kSbSocketOk;
  return true;
}

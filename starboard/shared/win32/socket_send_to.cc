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

int SbSocketSendTo(SbSocket socket,
                   const char* data,
                   int data_size,
                   const SbSocketAddress* destination) {
  const int kSendFlags = 0;
  if (!SbSocketIsValid(socket)) {
    return -1;
  }

  SB_DCHECK(socket->socket_handle != INVALID_SOCKET);
  if (socket->protocol == kSbSocketProtocolTcp) {
    if (destination) {
      SB_DLOG(FATAL) << "Destination passed to TCP send.";
      socket->error = kSbSocketErrorFailed;
      return -1;
    }

    int bytes_written =
        send(socket->socket_handle, data, data_size, kSendFlags);
    if (bytes_written >= 0) {
      socket->error = kSbSocketOk;
      return static_cast<int>(bytes_written);
    }

    int last_error = WSAGetLastError();
    if (last_error != EWOULDBLOCK) {
      SB_DLOG(ERROR) << "send failed, last_error = " << last_error;
    } else {
      // When EWOULDBLOCK is returned, the socket is no longer writable.
      socket->writable.store(false);
    }
    socket->error = sbwin32::TranslateSocketErrorStatus(last_error);
    return -1;
  } else if (socket->protocol == kSbSocketProtocolUdp) {
    if (!destination) {
      SB_DLOG(FATAL) << "No destination passed to UDP send.";
      socket->error = kSbSocketErrorFailed;
      return -1;
    }

    sbwin32::SockAddr sock_addr;
    const sockaddr* sockaddr = nullptr;
    socklen_t sockaddr_length = 0;
    if (destination) {
      if (!sock_addr.FromSbSocketAddress(destination)) {
        SB_DLOG(FATAL) << "Invalid destination passed to UDP send.";
        socket->error = kSbSocketErrorFailed;
        return -1;
      }
      sockaddr = sock_addr.sockaddr();
      sockaddr_length = sock_addr.length;
    }

    int bytes_written = sendto(socket->socket_handle, data, data_size,
                               kSendFlags, sockaddr, sockaddr_length);
    if (bytes_written >= 0) {
      socket->error = kSbSocketOk;
      return static_cast<int>(bytes_written);
    }

    int last_error = WSAGetLastError();
    if (last_error != WSAEWOULDBLOCK) {
      SB_DLOG(ERROR) << "sendto failed, last_error = " << last_error;
    }
    socket->error = sbwin32::TranslateSocketErrorStatus(last_error);
    return -1;
  }

  SB_NOTREACHED() << "Unrecognized protocol: " << socket->protocol;
  return -1;
}

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

namespace {

bool IsReportableErrno(int code) {
  return (code != WSAEWOULDBLOCK && code != WSAECONNRESET);
}

}  // namespace

int SbSocketReceiveFrom(SbSocket socket,
                        char* out_data,
                        int data_size,
                        SbSocketAddress* out_source) {
  const int kRecvFlags = 0;

  if (!SbSocketIsValid(socket)) {
    return -1;
  }

  SB_DCHECK(socket->socket_handle != INVALID_SOCKET);
  if (socket->protocol == kSbSocketProtocolTcp) {
    if (out_source) {
      sbwin32::SockAddr sock_addr;
      int result = getpeername(socket->socket_handle, sock_addr.sockaddr(),
                               &sock_addr.length);
      if (result == SOCKET_ERROR) {
        int last_error = WSAGetLastError();
        SB_DLOG(ERROR) << __FUNCTION__
                       << ": getpeername failed, last_error = " << last_error;
        socket->error = sbwin32::TranslateSocketErrorStatus(last_error);
        return -1;
      }

      if (!sock_addr.ToSbSocketAddress(out_source)) {
        SB_DLOG(FATAL) << __FUNCTION__ << ": Bad TCP source address.";
        socket->error = kSbSocketErrorFailed;
        return -1;
      }
    }

    int bytes_read =
        recv(socket->socket_handle, out_data, data_size, kRecvFlags);
    if (bytes_read >= 0) {
      socket->error = kSbSocketOk;
      return static_cast<int>(bytes_read);
    }

    int last_error = WSAGetLastError();
    if (IsReportableErrno(last_error) &&
        socket->error != sbwin32::TranslateSocketErrorStatus(last_error)) {
      SB_DLOG(ERROR) << "recv failed, last_error = " << last_error;
    }
    socket->error = sbwin32::TranslateSocketErrorStatus(last_error);
    return -1;
  } else if (socket->protocol == kSbSocketProtocolUdp) {
    sbwin32::SockAddr sock_addr;
    ssize_t bytes_read =
        recvfrom(socket->socket_handle, out_data, data_size, kRecvFlags,
                 sock_addr.sockaddr(), &sock_addr.length);

    if (bytes_read >= 0) {
      if (out_source) {
        if (!sock_addr.ToSbSocketAddress(out_source)) {
          SB_DLOG(FATAL) << __FUNCTION__ << ": Bad UDP source address.";
          socket->error = kSbSocketErrorFailed;
          return -1;
        }
      }

      socket->error = kSbSocketOk;
      return static_cast<int>(bytes_read);
    }

    int last_error = WSAGetLastError();
    if (last_error != WSAEWOULDBLOCK &&
        socket->error != sbwin32::TranslateSocketErrorStatus(last_error)) {
      SB_DLOG(ERROR) << "recvfrom failed, last_error = " << last_error;
    }
    socket->error = sbwin32::TranslateSocketErrorStatus(last_error);
    return -1;
  }

  SB_NOTREACHED() << "Unrecognized protocol: " << socket->protocol;
  return -1;
}

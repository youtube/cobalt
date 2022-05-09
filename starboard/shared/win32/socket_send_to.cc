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

#include <sstream>
#include <string>
#include <winsock2.h>

#include "starboard/common/log.h"
#include "starboard/shared/win32/socket_internal.h"

namespace sbwin32 = starboard::shared::win32;

namespace {
std::string CreateErrorString(const SbSocketAddress* addr, int error) {
  std::stringstream out;
  if (addr) {
    out << "Sendto failed while sending to port " << addr->port
        << ". Last Error: 0x" << std::hex << error;
  } else {
    out << "Sendto failed because of error: 0x" << std::hex << error;
  }
  return out.str();
}
}  // namespace

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

    if ((last_error == WSAEWOULDBLOCK) || (last_error == WSAECONNABORTED)) {
      socket->writable.store(false);
    } else {
      SB_DLOG(ERROR) << CreateErrorString(destination, last_error);
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
      SB_DLOG(ERROR) << CreateErrorString(destination, last_error);
    }
    socket->error = sbwin32::TranslateSocketErrorStatus(last_error);
    return -1;
  }

  SB_NOTREACHED() << "Unrecognized protocol: " << socket->protocol;
  return -1;
}

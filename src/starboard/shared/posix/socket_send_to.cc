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

#include "starboard/socket.h"

#include <errno.h>
#include <sys/socket.h>

#include "starboard/log.h"
#include "starboard/shared/posix/socket_internal.h"

namespace sbposix = starboard::shared::posix;

int SbSocketSendTo(SbSocket socket,
                   const char* data,
                   int data_size,
                   const SbSocketAddress* destination) {
#if defined(MSG_NOSIGNAL)
  const int kSendFlags = MSG_NOSIGNAL;
#else
  const int kSendFlags = 0;
#endif
  if (!SbSocketIsValid(socket)) {
    errno = EBADF;
    return -1;
  }

  SB_DCHECK(socket->socket_fd >= 0);
  if (socket->protocol == kSbSocketProtocolTcp) {
    if (destination) {
      SB_DLOG(FATAL) << "Destination passed to TCP send.";
      socket->error = kSbSocketErrorFailed;
      return -1;
    }

    ssize_t bytes_written =
        send(socket->socket_fd, data, data_size, kSendFlags);
    if (bytes_written >= 0) {
      socket->error = kSbSocketOk;
      return static_cast<int>(bytes_written);
    }

    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      SB_DLOG(ERROR) << "send failed, errno = " << errno;
    }
    socket->error = sbposix::TranslateSocketErrno(errno);
    return -1;
  } else if (socket->protocol == kSbSocketProtocolUdp) {
    if (!destination) {
      SB_DLOG(FATAL) << "No destination passed to UDP send.";
      socket->error = kSbSocketErrorFailed;
      return -1;
    }

    sbposix::SockAddr sock_addr;
    const sockaddr* sockaddr = NULL;
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

    ssize_t bytes_written = sendto(socket->socket_fd, data, data_size,
                                   kSendFlags, sockaddr, sockaddr_length);
    if (bytes_written >= 0) {
      socket->error = kSbSocketOk;
      return static_cast<int>(bytes_written);
    }

    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      SB_DLOG(ERROR) << "sendto failed, errno = " << errno;
    }
    socket->error = sbposix::TranslateSocketErrno(errno);
    return -1;
  }

  SB_NOTREACHED() << "Unrecognized protocol: " << socket->protocol;
  errno = EPROTONOSUPPORT;
  return -1;
}

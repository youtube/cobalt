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
#include "starboard/shared/posix/handle_eintr.h"
#include "starboard/shared/posix/socket_internal.h"

namespace sbposix = starboard::shared::posix;

namespace {

bool IsReportableErrno(int code) {
  return (code != EAGAIN && code != EWOULDBLOCK && code != ECONNRESET);
}

}  // namespace

int SbSocketReceiveFrom(SbSocket socket,
                        char* out_data,
                        int data_size,
                        SbSocketAddress* out_source) {
  const int kRecvFlags = 0;

  if (!SbSocketIsValid(socket)) {
    errno = EBADF;
    return -1;
  }

  SB_DCHECK(socket->socket_fd >= 0);
  if (socket->protocol == kSbSocketProtocolTcp) {
    if (out_source) {
      sbposix::SockAddr sock_addr;
      int result = getpeername(socket->socket_fd, sock_addr.sockaddr(),
                               &sock_addr.length);
      if (result < 0) {
        SB_DLOG(ERROR) << __FUNCTION__
                       << ": getpeername failed, errno = " << errno;
        socket->error = sbposix::TranslateSocketErrno(errno);
        return -1;
      }

      if (!sock_addr.ToSbSocketAddress(out_source)) {
        SB_DLOG(FATAL) << __FUNCTION__ << ": Bad TCP source address.";
        socket->error = kSbSocketErrorFailed;
        return -1;
      }
    }

    ssize_t bytes_read =
        recv(socket->socket_fd, out_data, data_size, kRecvFlags);
    if (bytes_read >= 0) {
      socket->error = kSbSocketOk;
      return static_cast<int>(bytes_read);
    }

    if (IsReportableErrno(errno) &&
        socket->error != sbposix::TranslateSocketErrno(errno)) {
      SB_DLOG(ERROR) << "recv failed, errno = " << errno;
    }
    socket->error = sbposix::TranslateSocketErrno(errno);
    return -1;
  } else if (socket->protocol == kSbSocketProtocolUdp) {
    sbposix::SockAddr sock_addr;
    ssize_t bytes_read =
        recvfrom(socket->socket_fd, out_data, data_size, kRecvFlags,
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

    if (errno != EAGAIN && errno != EWOULDBLOCK &&
        socket->error != sbposix::TranslateSocketErrno(errno)) {
      SB_DLOG(ERROR) << "recvfrom failed, errno = " << errno;
    }
    socket->error = sbposix::TranslateSocketErrno(errno);
    return -1;
  }

  SB_NOTREACHED() << "Unrecognized protocol: " << socket->protocol;
  errno = EPROTONOSUPPORT;
  return -1;
}

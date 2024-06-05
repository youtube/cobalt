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
#include <string.h>
#include <sys/socket.h>

#include "starboard/common/log.h"
#include "starboard/shared/posix/handle_eintr.h"
#include "starboard/shared/posix/socket_internal.h"

#ifndef UDP_GRO
#define UDP_GRO 104
#endif

#ifndef SOL_UDP
#define SOL_UDP 17
#endif

namespace sbposix = starboard::shared::posix;

namespace {

bool IsReportableErrno(int code) {
  return (code != EAGAIN && code != EWOULDBLOCK && code != ECONNRESET);
}

}  // namespace

static int recv_msg(int fd, char* buf, int len, int* gso_size) {
  char control[CMSG_SPACE(sizeof(int))] = {0};
  struct msghdr msg = {0};
  struct iovec iov = {0};
  struct cmsghdr* cmsg;
  int ret;

  iov.iov_base = buf;
  iov.iov_len = len;

  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  msg.msg_control = control;
  msg.msg_controllen = sizeof(control);

  *gso_size = -1;
  ret = recvmsg(fd, &msg, MSG_TRUNC | MSG_DONTWAIT);
  if (ret != -1) {
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
         cmsg = CMSG_NXTHDR(&msg, cmsg)) {
      if (cmsg->cmsg_level == SOL_UDP && cmsg->cmsg_type == UDP_GRO) {
        *gso_size = *(int*)CMSG_DATA(cmsg);
        // SB_LOG(INFO) << __FUNCTION__ << " gso_size=" << *gso_size;
        break;
      }
    }
  }
  return ret;
}

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
        SB_LOG(ERROR) << __FUNCTION__
                      << ": getpeername failed, errno = " << errno << " "
                      << strerror(errno);
        socket->error = sbposix::TranslateSocketErrno(errno);
        return -1;
      }

      if (!sock_addr.ToSbSocketAddress(out_source)) {
        SB_LOG(FATAL) << __FUNCTION__ << ": Bad TCP source address.";
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
      SB_LOG(ERROR) << "recv failed, errno = " << errno << " "
                    << strerror(errno);
    }
    socket->error = sbposix::TranslateSocketErrno(errno);
    return -1;
  } else if (socket->protocol == kSbSocketProtocolUdp) {
#if 0
    if (!out_source || (socket->message_idx < socket->message_count)) {
      // Batch read from the socket.
      // SB_LOG(INFO) << __FUNCTION__ << " // Batch read from the socket.";
      if (socket->message_idx >= socket->message_count) {
        int messages_retval = recvmmsg(socket->socket_fd, socket->msgs,
                                       SbSocketPrivate::kNumPackets,
                                       kRecvFlags | MSG_DONTWAIT, nullptr);
        if (messages_retval == -1) {
          if (errno != EAGAIN && errno != EWOULDBLOCK &&
              socket->error != sbposix::TranslateSocketErrno(errno)) {
            SB_LOG(ERROR) << "recvmmsg failed, errno = " << errno << " "
                          << strerror(errno);
          }
          socket->error = sbposix::TranslateSocketErrno(errno);
          return -1;
        }
        if (messages_retval == 0) {
          socket->error = kSbSocketOk;
          return 0;
        }
        socket->message_idx = 0;
        socket->message_count = messages_retval;
      }

      errno = 0;
      unsigned int bytes_read =
          std::min(static_cast<unsigned int>(data_size),
                   socket->msgs[socket->message_idx].msg_len);
      memcpy(out_data, socket->bufs[socket->message_idx], bytes_read);
      socket->message_idx++;
      return static_cast<int>(bytes_read);
    }
#endif
    sbposix::SockAddr sock_addr;
#if 0
    ssize_t bytes_read = recvfrom(socket->socket_fd, out_data, data_size,
                                  kRecvFlags | MSG_TRUNC | MSG_DONTWAIT,
                                  sock_addr.sockaddr(), &sock_addr.length);
#else
    ssize_t bytes_read = 0;
    if (socket->message_idx >= socket->message_count) {
      socket->gso_size = 0;
      bytes_read = recv_msg(socket->socket_fd, &socket->buffer[0],
                            socket->kBufferSize - 1, &socket->gso_size);
      if (bytes_read > 0) {
        if (socket->gso_size > 0) {
          socket->message_count = bytes_read;
          socket->message_idx = 0;
#if 0
          SB_LOG(INFO) << __FUNCTION__ << " bytes_read=" << bytes_read
                       << " data_size = " << data_size
                       << " gso_size=" << socket->gso_size;
#endif
        } else {
          bytes_read = std::min(static_cast<int>(data_size),
                                static_cast<int>(bytes_read));
          memcpy(out_data, socket->buffer, bytes_read);
        }
      }
    }
    if (socket->message_idx < socket->message_count) {
      errno = 0;
      unsigned int bytes_read = std::min(
          socket->message_count - socket->message_idx, socket->gso_size);
      memcpy(out_data, &socket->buffer[socket->message_idx],
             std::min(static_cast<unsigned int>(data_size), bytes_read));
      socket->message_idx += socket->gso_size;
      return static_cast<int>(bytes_read);
    }
#endif
    if (bytes_read >= 0) {
      if (out_source) {
        if (!sock_addr.ToSbSocketAddress(out_source)) {
          SB_LOG(FATAL) << __FUNCTION__ << ": Bad UDP source address.";
          socket->error = kSbSocketErrorFailed;
          return -1;
        }
      }

      socket->error = kSbSocketOk;
      return static_cast<int>(bytes_read);
    }

    if (errno != EAGAIN && errno != EWOULDBLOCK &&
        socket->error != sbposix::TranslateSocketErrno(errno)) {
      SB_LOG(ERROR) << "recvfrom failed, errno = " << errno << " "
                    << strerror(errno);
    }
    socket->error = sbposix::TranslateSocketErrno(errno);
    return -1;
  }

  SB_NOTREACHED() << "Unrecognized protocol: " << socket->protocol;
  errno = EPROTONOSUPPORT;
  return -1;
}

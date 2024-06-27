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

#ifndef UDP_GRO
#define UDP_GRO 104
#endif

#ifndef SOL_UDP
#define SOL_UDP 17
#endif

namespace sbposix = starboard::shared::posix;

SbSocketError SbSocketConnect(SbSocket socket, const SbSocketAddress* address) {
  if (!SbSocketIsValid(socket)) {
    return kSbSocketErrorFailed;
  }

  sbposix::SockAddr sock_addr;
  if (!sock_addr.FromSbSocketAddress(address)) {
    SB_LOG(ERROR) << __FUNCTION__ << ": Invalid address";
    return (socket->error = kSbSocketErrorFailed);
  }

  SB_DCHECK(socket->socket_fd >= 0);
  if (address->type != socket->address_type) {
    SB_LOG(ERROR) << __FUNCTION__ << ": Incompatible addresses: "
                  << "socket type = " << socket->address_type
                  << ", argument type = " << address->type;
    return (socket->error = kSbSocketErrorFailed);
  }

  int result = HANDLE_EINTR(
      connect(socket->socket_fd, sock_addr.sockaddr(), sock_addr.length));
  if (result != 0 && errno == EINPROGRESS) {
    return (socket->error = kSbSocketPending);
  }

  if (result != 0) {
    SB_LOG(ERROR) << __FUNCTION__ << ": connect failed: " << errno;
    return (socket->error = kSbSocketErrorFailed);
  }

  // https://lwn.net/Articles/768995/
  if (socket->protocol == kSbSocketProtocolUdp) {
    int udp_gro_value = 0;
    socklen_t udp_gro_value_len = sizeof(udp_gro_value);
    if (getsockopt(socket->socket_fd, SOL_UDP, UDP_GRO, &udp_gro_value,
                   &udp_gro_value_len) == 0) {
      SB_LOG(INFO) << "getsockopt SOL_UDP, UDP_GRO supported on this device.";
#if 1
      if (udp_gro_value == 0) {
        udp_gro_value = 1;  // Turn it on
        if (setsockopt(socket->socket_fd, SOL_UDP, UDP_GRO, &udp_gro_value,
                       sizeof(udp_gro_value))) {
          SB_LOG(INFO) << "setsockopt SOL_UDP, UDP_GRO failed: "
                       << strerror(errno);
        } else {
          SB_LOG(INFO) << "setsockopt SOL_UDP, UDP_GRO turned on.";
        }
      }
#endif
    } else {
      SB_LOG(INFO)
          << "getsockopt SOL_UDP, UDP_GRO not supported on this device: "
          << strerror(errno);
    }
  }

  return (socket->error = kSbSocketOk);
}

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

#include <arpa/inet.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "starboard/common/log.h"
#include "starboard/shared/posix/socket_internal.h"

namespace sbposix = starboard::shared::posix;

bool SbSocketJoinMulticastGroup(SbSocket socket,
                                const SbSocketAddress* address) {
  if (!SbSocketIsValid(socket)) {
    errno = EBADF;
    return false;
  }

  if (!address) {
    errno = EINVAL;
    return false;
  }

  SB_DCHECK(socket->socket_fd >= 0);
  struct ip_mreq imreq = {0};
  in_addr_t in_addr = *reinterpret_cast<const in_addr_t*>(address->address);
  imreq.imr_multiaddr.s_addr = in_addr;
  imreq.imr_interface.s_addr = INADDR_ANY;

  int result = setsockopt(socket->socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                          &imreq, sizeof(imreq));
  if (result != 0) {
    SB_DLOG(ERROR) << "Failed to IP_ADD_MEMBERSHIP on socket "
                   << socket->socket_fd << ", errno = " << errno;
    socket->error = sbposix::TranslateSocketErrno(errno);
    return false;
  }

  socket->error = kSbSocketOk;
  return true;
}

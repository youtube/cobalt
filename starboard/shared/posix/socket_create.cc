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
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "starboard/log.h"
#include "starboard/shared/posix/handle_eintr.h"
#include "starboard/shared/posix/set_non_blocking_internal.h"
#include "starboard/shared/posix/socket_internal.h"

namespace sbposix = starboard::shared::posix;

SbSocket SbSocketCreate(SbSocketAddressType address_type,
                        SbSocketProtocol protocol) {
  int socket_domain;
  switch (address_type) {
    case kSbSocketAddressTypeIpv4:
      socket_domain = AF_INET;
      break;
    case kSbSocketAddressTypeIpv6:
      socket_domain = AF_INET6;
      break;
    default:
      SB_NOTREACHED();
      errno = EAFNOSUPPORT;
      return kSbSocketInvalid;
  }

  int socket_type;
  int socket_protocol;
  switch (protocol) {
    case kSbSocketProtocolTcp:
      socket_type = SOCK_STREAM;
      socket_protocol = IPPROTO_TCP;
      break;
    case kSbSocketProtocolUdp:
      socket_type = SOCK_DGRAM;
      socket_protocol = IPPROTO_UDP;
      break;
    default:
      SB_NOTREACHED();
      errno = EPROTONOSUPPORT;
      return kSbSocketInvalid;
  }

  int socket_fd = ::socket(socket_domain, socket_type, socket_protocol);
  if (socket_fd < 0) {
    return kSbSocketInvalid;
  }

  // All Starboard sockets are non-blocking, so let's ensure it.
  if (!sbposix::SetNonBlocking(socket_fd)) {
    // Something went wrong, we'll clean up (preserving errno) and return
    // failure.
    int save_errno = errno;
    HANDLE_EINTR(close(socket_fd));
    errno = save_errno;
    return kSbSocketInvalid;
  }

#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
  // Use SO_NOSIGPIPE to mute SIGPIPE on darwin systems.
  int optval_set=1;
  setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE, (void*)&optval_set,
    sizeof(int));
#endif

  return new SbSocketPrivate(address_type, protocol, socket_fd);
}

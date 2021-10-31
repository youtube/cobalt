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
#include <sys/types.h>

#include "starboard/common/log.h"
#include "starboard/shared/posix/socket_internal.h"

namespace sbposix = starboard::shared::posix;

SbSocketError SbSocketListen(SbSocket socket) {
  if (!SbSocketIsValid(socket)) {
    errno = EBADF;
    return kSbSocketErrorFailed;
  }

  SB_DCHECK(socket->socket_fd >= 0);
  // We set the backlog to SOMAXCONN to ensure that it is above 1, and high
  // enough that all tests are able to pass. Some tests will fail if backlog==1
  // because they expect to be able to successfully initiate multiple connects
  // at once, and then after all connects have been initiated to subsequently
  // initiate corresponding accepts.
#if defined(SOMAXCONN)
  const int kMaxConn = SOMAXCONN;
#else
  // Some posix platforms such as FreeBSD do not define SOMAXCONN.
  // In this case, set the value to an arbitrary number large enough to
  // satisfy most use-cases and tests, empirically we have found that 128
  // is sufficient.  All implementations of listen() specify that a backlog
  // parameter larger than the system max will be silently truncated to the
  // system's max.
  const int kMaxConn = 128;
#endif
  int result = listen(socket->socket_fd, kMaxConn);
  if (result != 0) {
    return (socket->error = sbposix::TranslateSocketErrno(result));
  }

  return (socket->error = kSbSocketOk);
}

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

// Adapted from net_errors_posix.h

#if SB_API_VERSION >= 16
#include <errno.h>
#include <string.h>
#endif

#include "net/base/net_errors.h"

#include "base/notreached.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "starboard/system.h"
#include "starboard/socket.h"

namespace net {

Error MapSystemError(logging::SystemErrorCode error) {
  if (error != 0) {
    char error_string[256];
    SbSystemGetErrorString(error, error_string, sizeof(error_string));
    DVLOG(2) << "Error (" << error << ") " << error_string;
  }

  // TODO: Define standard Starboard error codes.
  if (error == 0)
    return OK;

  return ERR_FAILED;
}

Error MapSocketError(SbSocketError error) {
  DLOG(ERROR) << "errno test failed MapSocketError: " << error;
  if (error != kSbSocketOk)
    DVLOG(2) << "Error " << error;

  // TODO: Define standard Starboard error codes.
  switch (error) {
    case kSbSocketOk:
      return OK;
    case kSbSocketPending:
      return ERR_IO_PENDING;
    case kSbSocketErrorConnectionReset:
      return ERR_CONNECTION_RESET;
    case kSbSocketErrorFailed:
      return ERR_FAILED;
    default:
      NOTREACHED() << "Unrecognized error: " << error;
      return ERR_FAILED;
  }
}

#if SB_API_VERSION >= 16
Error MapLastPosixSocketError() {
  DLOG(ERROR) << "errno test failed MapLastPosixSocketError: " << errno;
  if (errno != 0)
    DLOG(ERROR) << "Error " << strerror(errno);

  switch (errno) {
    case 0:
      DLOG(ERROR) << "errno is 0" << errno;
    case EINPROGRESS:
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
      return ERR_IO_PENDING;
    case ECONNRESET:
    case ENETRESET:
    case EPIPE:
      return ERR_CONNECTION_RESET;
    default:
      // Here's where we would be more nuanced if we need to be.
      return ERR_FAILED;
  }
}
#endif // SB_API_VERSION >= 16

}  // namespace net

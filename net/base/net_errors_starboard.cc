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

#include "net/base/net_errors.h"

#include "base/logging.h"
#include "base/stringprintf.h"
#include "starboard/system.h"

namespace net {

Error MapSystemError(SbSystemError error) {
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
  if (error != kSbSocketOk)
    DVLOG(2) << "Error " << error;

  // TODO: Define standard Starboard error codes.
  switch (error) {
    case kSbSocketOk:
      return OK;
    case kSbSocketPending:
      return ERR_IO_PENDING;
#if SB_API_VERSION >= SB_ADDITIONAL_SOCKET_CONNECTION_ERRORS_API_VERSION
    case kSbSocketErrorConnectionReset:
      return ERR_CONNECTION_RESET;
#endif  // SB_API_VERSION >= SB_ADDITIONAL_SOCKET_CONNECTION_ERRORS_API_VERSION
    case kSbSocketErrorFailed:
      return ERR_FAILED;
    default:
      NOTREACHED() << "Unrecognized error: " << error;
      return ERR_FAILED;
  }
}

}  // namespace net

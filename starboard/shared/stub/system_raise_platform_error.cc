// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/system.h"

#include "starboard/log.h"

SbSystemPlatformError SbSystemRaisePlatformError(
    SbSystemPlatformErrorType type,
    SbSystemPlatformErrorCallback callback,
    void* user_data) {
  SB_UNREFERENCED_PARAMETER(callback);
  SB_UNREFERENCED_PARAMETER(user_data);
  std::string message;
  switch (type) {
    case kSbSystemPlatformErrorTypeConnectionError:
      message = "Connection error.";
      break;
#if SB_API_VERSION < SB_PLATFORM_ERROR_CLEANUP_API_VERSION
    case kSbSystemPlatformErrorTypeUserSignedOut:
      message = "User is not signed in.";
      break;
    case kSbSystemPlatformErrorTypeUserAgeRestricted:
      message = "User is age restricted.";
      break;
#endif
    default:
      message = "<unknown>";
      break;
  }
  SB_DLOG(INFO) << "SbSystemRaisePlatformError: " << message;
  return kSbSystemPlatformErrorInvalid;
}

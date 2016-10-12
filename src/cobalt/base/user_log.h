/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_BASE_USER_LOG_H_
#define COBALT_BASE_USER_LOG_H_

#include "base/basictypes.h"

namespace base {

class UserLog {
 public:
  static const int kEventStackMaxDepth = 10;
  enum Index {
    kUserAgentStringIndex = 0,
    kSystemLanguageStringIndex,
    kAvailableMemoryIndex,
    kAppLifetimeIndex,
    kAppStatusIndex,
    kAppSuspendCountIndex,
    kAppResumeCountIndex,
    kAppPauseCountIndex,
    kAppUnpauseCountIndex,
    kNetworkStatusIndex,
    kNetworkConnectCountIndex,
    kNetworkDisconnectCountIndex,
    kWebMediaPlayerState,
    kNodeCountIndex,
    kElementCountIndex,
    kHtmlElementCountIndex,
    kHTMLMediaElementCountIndex,
    kEventStackLevelCountIndex,
    kEventStackMinLevelIndex,
    kEventStackMaxLevelIndex =
        kEventStackMinLevelIndex + kEventStackMaxDepth - 1,
    // Valid indices are from [0, 127].
    kMaxIndex = 128,
  };

  static bool IsRegistrationSupported();

  // Thread-safe registration of a user log index, associating it with the
  // specified label and memory address. This is used by some platforms to
  // provide additional context with crashes.
  //
  // Returns true if the registration is successful.
  //
  // NOTE: The max size of the label is 15 ASCII characters, plus a NULL
  // character.
  static bool Register(Index index, const char* label, const void* address,
                       size_t size);

  // Deregister the given Index so that it is not included in the crash report.
  static bool Deregister(Index index);
};

}  // namespace base

#endif  // COBALT_BASE_USER_LOG_H_

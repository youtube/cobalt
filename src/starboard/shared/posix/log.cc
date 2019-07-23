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

#include "starboard/common/log.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if SB_API_VERSION >= 11
#include "starboard/shared/starboard/log_mutex.h"
#endif  // SB_API_VERSION >= 11

void SbLog(SbLogPriority priority, const char* message) {
  SB_UNREFERENCED_PARAMETER(priority);
#if SB_API_VERSION < 11
  fprintf(stderr, "%s", message);
  fflush(stderr);
#else   // SB_API_VERSION >= 11
  starboard::shared::starboard::GetLoggingMutex()->Acquire();
  fprintf(stderr, "%s", message);
  fflush(stderr);
  starboard::shared::starboard::GetLoggingMutex()->Release();
#endif  // SB_API_VERSION < 11
}

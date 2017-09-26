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

#include <android/log.h>
#include <stdio.h>
#include <unistd.h>

#include <string>

#include "starboard/android/shared/log_file_impl.h"
#include "starboard/log.h"

using starboard::android::shared::WriteToLogFile;

void SbLog(SbLogPriority priority, const char* message) {
  int android_priority;
  switch (priority) {
    case kSbLogPriorityUnknown:
      android_priority = ANDROID_LOG_UNKNOWN;
      break;
    case kSbLogPriorityInfo:
      android_priority = ANDROID_LOG_INFO;
      break;
    case kSbLogPriorityWarning:
      android_priority = ANDROID_LOG_WARN;
      break;
    case kSbLogPriorityError:
      android_priority = ANDROID_LOG_ERROR;
      break;
    case kSbLogPriorityFatal:
      android_priority = ANDROID_LOG_FATAL;
      break;
    default:
      android_priority = ANDROID_LOG_INFO;
      break;
  }
  __android_log_write(android_priority, "starboard", message);

  std::string message_str(message);

  WriteToLogFile(message_str.c_str());
}

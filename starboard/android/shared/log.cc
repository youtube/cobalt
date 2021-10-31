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

#include <android/log.h>
#include <stdio.h>
#include <unistd.h>

#include <jni.h>
#include <string>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/android/shared/log_internal.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/shared/starboard/log_mutex.h"
#include "starboard/thread.h"

using starboard::android::shared::JniEnvExt;
using starboard::android::shared::ScopedLocalJavaRef;

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

  starboard::shared::starboard::GetLoggingMutex()->Acquire();
  __android_log_write(android_priority, "starboard", message);
  starboard::shared::starboard::GetLoggingMutex()->Release();

  // In unit tests the logging is too fast for the android log to be read out
  // and we end up losing crucial logs. The test runner specifies a sleep time.
  SbThreadSleep(::starboard::android::shared::GetLogSleepTime());
}

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
#include <unistd.h>

#include "starboard/android/shared/log_internal.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"

void SbLogRaw(const char* message) {
  __android_log_write(ANDROID_LOG_INFO, "starboard", message);
  usleep(::starboard::android::shared::GetLogSleepTime());
}

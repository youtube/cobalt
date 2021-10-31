// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/log_internal.h"

#include <stdlib.h>

#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace {
const char kLogSleepTimeSwitch[] = "android_log_sleep_time";
SbTime g_log_sleep_time = 0;
}  // namespace

namespace starboard {
namespace android {
namespace shared {

void LogInit(const starboard::shared::starboard::CommandLine& command_line) {
  if (command_line.HasSwitch(kLogSleepTimeSwitch)) {
    g_log_sleep_time =
        atol(command_line.GetSwitchValue(kLogSleepTimeSwitch).c_str());
    SB_LOG(INFO) << "Android log sleep time: " << g_log_sleep_time;
  }
}

SbTime GetLogSleepTime() {
  return g_log_sleep_time;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

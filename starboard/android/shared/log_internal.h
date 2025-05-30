// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

// This header provides a mechanism for multiple Android logging
// formats to share a single log file handle.

#ifndef STARBOARD_ANDROID_SHARED_LOG_INTERNAL_H_
#define STARBOARD_ANDROID_SHARED_LOG_INTERNAL_H_

#include "starboard/common/command_line.h"

namespace starboard::android::shared {

void LogInit(const starboard::CommandLine& command_line);
int64_t GetLogSleepTime();  // Returns microseconds

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_LOG_INTERNAL_H_

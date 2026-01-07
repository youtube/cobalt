// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_PERFORMANCE_STARTUP_TIME_H_
#define COBALT_BROWSER_PERFORMANCE_STARTUP_TIME_H_

#include "starboard/types.h"

namespace cobalt {
namespace browser {

void SetStartupTime(int64_t startup_time);
void SetStartupTime1(int64_t startup_time);
int64_t GetStartupTime();
int64_t GetStartupTime1();

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_PERFORMANCE_STARTUP_TIME_H_

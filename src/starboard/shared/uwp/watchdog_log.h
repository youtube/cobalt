// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_UWP_WATCHDOG_LOG_H_
#define STARBOARD_SHARED_UWP_WATCHDOG_LOG_H_

#include <string>

namespace starboard {
namespace shared {
namespace uwp {

// Starts a watch dog log file. This file has "alive" printed
// to it periodically and "done" when it finishes. Useful for
// tests to determine when the process exits. Only one watchdog
// log can be active in the process.
void StartWatchdogLog(const std::string& path);
void CloseWatchdogLog();

}  // namespace uwp
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_UWP_WATCHDOG_LOG_H_

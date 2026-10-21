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

#include "components/crash/core/app/crashpad.h"

namespace crash_reporter {

void SetFirstChanceExceptionHandler(bool (*handler)(int, siginfo_t*, void*)) {}

bool GetHandlerSocket(int*, pid_t*) {
  return false;
}

namespace internal {

bool PlatformCrashpadInitialization(bool,
                                    bool,
                                    bool,
                                    const std::string&,
                                    const base::FilePath&,
                                    const std::vector<std::string>&,
                                    base::FilePath*) {
  return false;
}

}  // namespace internal
}  // namespace crash_reporter

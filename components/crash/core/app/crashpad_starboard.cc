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

// Crash reporting in the hermetic libcobalt.so is a no-op: the loader installs
// Crashpad by calling //starboard/crashpad_wrapper directly, and libcobalt.so
// reaches it only through the CrashHandler Starboard extension, never through
// this API.

#include "components/crash/core/app/crashpad.h"

#include "base/notimplemented.h"

namespace crash_reporter {

bool InitializeCrashpad(bool initial_client, const std::string& process_type) {
  NOTIMPLEMENTED();
  return false;
}

void SetUploadConsent(bool consent) {
  NOTIMPLEMENTED();
}

void SetFirstChanceExceptionHandler(bool (*handler)(int, siginfo_t*, void*)) {
  NOTIMPLEMENTED();
}

bool GetHandlerSocket(int*, pid_t*) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace crash_reporter

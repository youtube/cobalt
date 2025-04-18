// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

// Provides stubs for base symbols required by the linker.

#include <limits.h>  // For INT_MAX
#include <stddef.h>  // For size_t

#include "base/notreached.h"
#include "base/process/process.h"

namespace base {

// Stub for base::GetMaxFds()
size_t GetMaxFds() {
  NOTIMPLEMENTED();
  return 0;
}

// Stub for base::IncreaseFdLimitTo(unsigned int)
void IncreaseFdLimitTo(unsigned int max_descriptors) {
  NOTIMPLEMENTED();
  // No action needed for a stub.
}

// Stub for base::Process::CanBackgroundProcesses()
// static
bool Process::CanBackgroundProcesses() {
  NOTIMPLEMENTED();
  // Return false as a safe default, indicating backgrounding is not supported.
  return false;
}

// Stub for base::Process::SetProcessBackgrounded(bool)
bool Process::SetProcessBackgrounded(bool background) {
  NOTIMPLEMENTED();
  // Return false as a safe default, indicating the operation failed or is not
  // supported.
  return false;
}

// --- Add other necessary stubs below ---

}  // namespace base

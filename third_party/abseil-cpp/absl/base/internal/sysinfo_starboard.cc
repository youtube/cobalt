// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/base/internal/sysinfo.h"

#include "absl/base/attributes.h"

#include "starboard/system.h"
#include "starboard/thread.h"

namespace absl {
ABSL_NAMESPACE_BEGIN
namespace base_internal {

// NominalCPUFrequency() may be called before main() and before malloc is
// properly initialized, therefore this must not allocate memory.
double NominalCPUFrequency() {
  return 1.0;
}

// NumCPUs() may be called before main() and before malloc is properly
// initialized, therefore this must not allocate memory.
int NumCPUs() {
  return SbSystemGetNumberOfProcessors();
}

// Return a per-thread small integer ID from pthread's thread-specific data.
pid_t GetTID() {
  return SbThreadGetId();
}

// GetCachedTID() caches the thread ID in thread-local storage (which is a
// userspace construct) to avoid unnecessary system calls. Without this caching,
// it can take roughly 98ns, while it takes roughly 1ns with this caching.
pid_t GetCachedTID() {
  return GetTID();
}

}  // namespace base_internal
ABSL_NAMESPACE_END
}  // namespace absl

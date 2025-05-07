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

#include "base/process/process.h"
#if BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)
#include "base/starboard/linker_stub.h"
#endif  // BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)

namespace base {

// TODO: b/412763867 - Cobalt: Make base changes which can
// be upstreamed to Chromium.
// Note: This is a stub implementation. The actual implementation is in
// process_metrics_posix.cc which is not included in the cobalt build.
// Stub symbols have been provided here to satisfy the linker and build cobalt

size_t GetMaxFds() {
  COBALT_LINKER_STUB();
  return 0;
}

// Stub for base::IncreaseFdLimitTo(unsigned int)
void IncreaseFdLimitTo(unsigned int max_descriptors) {
  COBALT_LINKER_STUB();
}

bool Process::CanBackgroundProcesses() {
  COBALT_LINKER_STUB();
  return false;
}

}  // namespace base

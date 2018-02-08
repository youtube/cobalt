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

#include "base/debug/stack_trace.h"

#include "base/logging.h"
#include "starboard/system.h"

namespace base {
namespace debug {

bool SpawnDebuggerOnProcess(unsigned process_id) {
  NOTIMPLEMENTED();
  return false;
}

bool BeingDebugged() {
#if defined(COBALT_BUILD_TYPE_GOLD)
  return false;
#else
  return SbSystemIsDebuggerAttached();
#endif
}

void BreakDebugger() {
  SbSystemBreakIntoDebugger();
}

}  // namespace debug
}  // namespace base

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

#include "src/base/debug/stack_trace.h"

#include "src/base/build_config.h"
#include "src/base/free_deleter.h"
#include "src/base/logging.h"
#include "src/base/macros.h"

#include "starboard/log.h"

namespace v8 {
namespace base {
namespace debug {

bool EnableInProcessStackDumping() {
  SB_NOTIMPLEMENTED();
  return false;
}

void DisableSignalStackDump() {
  SB_NOTIMPLEMENTED();
}

StackTrace::StackTrace() {
  SB_NOTIMPLEMENTED();
}

void StackTrace::Print() const {
  SB_NOTIMPLEMENTED();
}

void StackTrace::OutputToStream(std::ostream* os) const {
  SB_NOTIMPLEMENTED();
}

}  // namespace debug
}  // namespace base
}  // namespace v8
// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/script/v8c/switches.h"

namespace cobalt {
namespace script {
namespace v8c {
namespace switches {

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
// Runtime static flags that are set for all v8 instances in this run.
// Check v8's flag-definitions.h for the list of configurable flags.
// e.g. ./cobalt --v8_flags="--jitless --trace_gc"
const char kV8Flags[] = "v8_flags";
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

}  // namespace switches
}  // namespace v8c
}  // namespace script
}  // namespace cobalt

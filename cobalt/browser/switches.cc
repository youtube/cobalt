/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "switches.h"

namespace cobalt {
namespace browser {
namespace switches {

// Setting this switch defines the startup URL that Cobalt will use.  If no
// value is set, a default URL will be used.
const char kInitialURL[] = "url";

// If this is set, then a trace (see base/debug/trace_eventh.h) is started on
// Cobalt startup.  A value must also be specified for this switch, which is
// the duration in seconds of how long the trace will be done for before ending
// and saving the results to disk.  Results will be saved to the file
// "timed_trace.json" in the log output directory.
const char kTimedTrace[] = "timed_trace";

// If this flag is set, input will be continuously generated randomly instead of
// taken from an external input device (like a controller).
const char kInputFuzzer[] = "input_fuzzer";

}  // namespace switches
}  // namespace browser
}  // namespace cobalt

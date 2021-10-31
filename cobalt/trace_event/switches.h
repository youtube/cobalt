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

#ifndef COBALT_TRACE_EVENT_SWITCHES_H_
#define COBALT_TRACE_EVENT_SWITCHES_H_

namespace cobalt {
namespace trace_event {
namespace switches {

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
extern const char kLogTimedTrace[];
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

}  // namespace switches
}  // namespace trace_event
}  // namespace cobalt

#endif  // COBALT_TRACE_EVENT_SWITCHES_H_

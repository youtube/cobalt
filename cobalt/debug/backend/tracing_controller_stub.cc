// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include <utility>

#include "base/tracing_buildflags.h"
#include "cobalt/debug/backend/tracing_controller.h"
#include "cobalt/script/script_debugger.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#error This is a stub that should only be included in builds without tracing.
#endif

namespace cobalt {
namespace debug {
namespace backend {

namespace {

constexpr char kInspectorDomain[] = "Tracing";

}  // namespace

TracingController::TracingController(DebugDispatcher* dispatcher,
                                     script::ScriptDebugger* script_debugger)
    : dispatcher_(dispatcher),
      script_debugger_(script_debugger),
      tracing_started_(false),
      collected_size_(0),
      commands_(kInspectorDomain) {}

void TracingController::Thaw(JSONObject agent_state) {}

JSONObject TracingController::Freeze() { return JSONObject(); }

void TracingController::End(Command command) {}

void TracingController::Start(Command command) {}

void TracingController::AppendTraceEvent(const std::string& trace_event_json) {}

void TracingController::FlushTraceEvents() {}

void TracingController::SendDataCollectedEvent() {}

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

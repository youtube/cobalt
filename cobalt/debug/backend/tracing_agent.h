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

#ifndef COBALT_DEBUG_BACKEND_TRACING_AGENT_H_
#define COBALT_DEBUG_BACKEND_TRACING_AGENT_H_

#include <string>
#include <vector>

#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "cobalt/debug/backend/command_map.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/command.h"
#include "cobalt/script/script_debugger.h"

namespace cobalt {
namespace debug {
namespace backend {

// There aren't enable/disable commands in the Tracing domain, so the
// TracingAgent doesn't use AgentBase.
//
// https://chromedevtools.github.io/devtools-protocol/tot/Tracing
class TracingAgent : public script::ScriptDebugger::TraceDelegate  {
 public:
  explicit TracingAgent(DebugDispatcher* dispatcher,
                        script::ScriptDebugger* script_debugger);

  void Thaw(JSONObject agent_state);
  JSONObject Freeze();

  // TraceDelegate
  void AppendTraceEvent(const std::string& trace_event_json) override;
  void FlushTraceEvents() override;

 private:
  void End(Command command);
  void Start(Command command);

  void SendDataCollectedEvent();

  DebugDispatcher* dispatcher_;
  script::ScriptDebugger* script_debugger_;

  THREAD_CHECKER(thread_checker_);

  bool tracing_started_;
  std::vector<std::string> categories_;
  size_t collected_size_;
  JSONList collected_events_;

  // Map of member functions implementing commands.
  CommandMap commands_;
};

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_TRACING_AGENT_H_

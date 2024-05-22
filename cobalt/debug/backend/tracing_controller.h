// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_DEBUG_BACKEND_TRACING_CONTROLLER_H_
#define COBALT_DEBUG_BACKEND_TRACING_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/tracing_buildflags.h"
#include "cobalt/debug/backend/command_map.h"
#include "cobalt/debug/backend/debug_dispatcher.h"
#include "cobalt/debug/command.h"
#include "cobalt/script/script_debugger.h"

#if BUILDFLAG(ENABLE_BASE_TRACING)
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/tracing_agent.h"
#endif

namespace cobalt {
namespace debug {
namespace backend {

#if BUILDFLAG(ENABLE_BASE_TRACING)

using base::trace_event::TraceConfig;
using base::trace_event::TracingAgent;

//////////////////////////////////////////////////////////////////////////////

class TraceEventAgent : public TracingAgent {
 public:
  TraceEventAgent() = default;
  ~TraceEventAgent() = default;

  // TracingAgent Interface
  std::string GetTracingAgentName() override { return agent_name_; }
  std::string GetTraceEventLabel() override { return agent_event_label_; }
  void StartAgentTracing(const TraceConfig& trace_config,
                         StartAgentTracingCallback callback) override;
  void StopAgentTracing(StopAgentTracingCallback callback) override;

  void CancelTracing();

 private:
  void CollectTraceData(
      base::OnceClosure finished_cb,
      const scoped_refptr<base::RefCountedString>& event_string,
      bool has_more_events);

 private:
  std::string agent_name_{"TraceEventAgent"};
  std::string agent_event_label_{"Performance Tracing"};

  base::trace_event::TraceResultBuffer trace_buffer_;
  base::trace_event::TraceResultBuffer::SimpleOutput json_output_;
};

////////////////////////////////////////////////////////////////////////////////

class TraceV8Agent : public TracingAgent,
                     public script::ScriptDebugger::TraceDelegate {
 public:
  explicit TraceV8Agent(script::ScriptDebugger* script_debugger);
  ~TraceV8Agent() = default;

  // TraceDelegate Interface
  void AppendTraceEvent(const std::string& trace_event_json) override;
  void FlushTraceEvents() override;

  // TracingAgent Interface
  std::string GetTracingAgentName() override { return agent_name_; }
  std::string GetTraceEventLabel() override { return agent_event_label_; }
  void StartAgentTracing(const TraceConfig& trace_config,
                         StartAgentTracingCallback callback) override;
  void StopAgentTracing(StopAgentTracingCallback callback) override;

 private:
  std::string agent_name_{"TraceV8Agent"};
  std::string agent_event_label_{"Performance Tracing"};

  StopAgentTracingCallback on_stop_callback_;

  THREAD_CHECKER(thread_checker_);
  script::ScriptDebugger* script_debugger_;

  // size_t collected_size_;
  // JSONList collected_events_;
  base::trace_event::TraceResultBuffer trace_buffer_;
  base::trace_event::TraceResultBuffer::SimpleOutput json_output_;
};

//////////////////////////////////////////////////////////////////////////////
// https://chromedevtools.github.io/devtools-protocol/tot/Tracing
class TracingController {
 public:
  explicit TracingController(DebugDispatcher* dispatcher,
                             script::ScriptDebugger* script_debugger);
  ~TracingController() = default;

  void Thaw(JSONObject agent_state);
  JSONObject Freeze();

 private:
  void End(Command command);
  void Start(Command command);

  void OnStartTracing(const std::string& agent_name, bool success);
  void OnStopTracing(
      const std::string& agent_name, const std::string& events_label,
      const scoped_refptr<base::RefCountedString>& events_str_ptr);
  void OnCancelTracing(
      const std::string& agent_name, const std::string& events_label,
      const scoped_refptr<base::RefCountedString>& events_str_ptr);


 private:
  void SendDataCollectedEvent();
  void FlushTraceEvents();

 private:
  DebugDispatcher* dispatcher_;
  std::vector<std::unique_ptr<TracingAgent> > agents_;
  std::atomic_int agents_responded_{0};

  bool tracing_started_;
  std::vector<std::string> categories_;

  size_t collected_size_;
  JSONList collected_events_;

  // Map of member functions implementing commands.
  CommandMap commands_;
};

#else

// There aren't enable/disable commands in the Tracing domain, so the
// TracingController doesn't use AgentBase.
//
// https://chromedevtools.github.io/devtools-protocol/tot/Tracing
class TracingController : public script::ScriptDebugger::TraceDelegate {
 public:
  explicit TracingController(DebugDispatcher* dispatcher,
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

#endif

}  // namespace backend
}  // namespace debug
}  // namespace cobalt

#endif  // COBALT_DEBUG_BACKEND_TRACING_CONTROLLER_H_

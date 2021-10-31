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

#ifndef COBALT_SCRIPT_V8C_V8C_TRACING_CONTROLLER_H_
#define COBALT_SCRIPT_V8C_V8C_TRACING_CONTROLLER_H_

#include <string>
#include <vector>

#include "cobalt/script/script_debugger.h"
#include "v8/include/libplatform/v8-tracing.h"

namespace cobalt {
namespace script {
namespace v8c {

// A |TracingController| that proxies all calls to an instance of V8's shared
// platform |TracingController| implementation, which is initialized to send
// trace events to our |TraceWriter| that forwards them to a |TraceDelegate|
// through Cobalt's |ScriptDebugger|.
class V8cTracingController : public v8::TracingController {
 public:
  V8cTracingController();

  void StartTracing(const std::vector<std::string>& categories,
                    ScriptDebugger::TraceDelegate* trace_delegate);
  void StopTracing();

  ScriptDebugger::TraceDelegate* GetTraceDelegate() { return trace_delegate_; }

  const uint8_t* GetCategoryGroupEnabled(const char* name) override {
    return v8_tracing_->GetCategoryGroupEnabled(name);
  }

  uint64_t AddTraceEvent(
      char phase, const uint8_t* category_enabled_flag, const char* name,
      const char* scope, uint64_t id, uint64_t bind_id, int32_t num_args,
      const char** arg_names, const uint8_t* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
      unsigned int flags) override {
    return v8_tracing_->AddTraceEvent(
        phase, category_enabled_flag, name, scope, id, bind_id, num_args,
        arg_names, arg_types, arg_values, arg_convertables, flags);
  }

  uint64_t AddTraceEventWithTimestamp(
      char phase, const uint8_t* category_enabled_flag, const char* name,
      const char* scope, uint64_t id, uint64_t bind_id, int32_t num_args,
      const char** arg_names, const uint8_t* arg_types,
      const uint64_t* arg_values,
      std::unique_ptr<v8::ConvertableToTraceFormat>* arg_convertables,
      unsigned int flags, int64_t timestamp) override {
    return v8_tracing_->AddTraceEventWithTimestamp(
        phase, category_enabled_flag, name, scope, id, bind_id, num_args,
        arg_names, arg_types, arg_values, arg_convertables, flags, timestamp);
  }

  void UpdateTraceEventDuration(const uint8_t* category_enabled_flag,
                                const char* name, uint64_t handle) override {
    v8_tracing_->UpdateTraceEventDuration(category_enabled_flag, name, handle);
  }

  void AddTraceStateObserver(TraceStateObserver* observer) override {
    v8_tracing_->AddTraceStateObserver(observer);
  }

  void RemoveTraceStateObserver(TraceStateObserver* observer) override {
    v8_tracing_->RemoveTraceStateObserver(observer);
  }

 private:
  // A V8 |TraceWriter| that forwards to the current Cobalt |TraceDelegate|.
  class TraceWriter : public v8::platform::tracing::TraceWriter {
   public:
    explicit TraceWriter(V8cTracingController* controller)
        : controller_(controller) {}
    void AppendTraceEvent(
        v8::platform::tracing::TraceObject* trace_event) override;
    void Flush() override;

   private:
    V8cTracingController* controller_;
  };

  std::unique_ptr<v8::platform::tracing::TracingController> v8_tracing_;
  ScriptDebugger::TraceDelegate* trace_delegate_;

  // Disallow copy and assign
  V8cTracingController(const V8cTracingController&) = delete;
  void operator=(const V8cTracingController&) = delete;
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_TRACING_CONTROLLER_H_

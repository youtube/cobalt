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

#include "cobalt/script/v8c/v8c_tracing_controller.h"

#include <sstream>

#include "v8/include/libplatform/v8-tracing.h"

namespace cobalt {
namespace script {
namespace v8c {

V8cTracingController::V8cTracingController()
    : v8_tracing_(new v8::platform::tracing::TracingController()) {
  // Initialize the V8 |TracingController| with a |TraceBufferRingBuffer| that
  // flushes the trace events to our own |TraceWriter|. Using V8's |TraceBuffer|
  // implementation saves us from having to implement our own, in particular the
  // trouble of |GetEventByHandle| that allows V8 to update the duration of
  // already-written events when they end.
  v8::platform::tracing::TraceBuffer* trace_buffer =
      v8::platform::tracing::TraceBuffer::CreateTraceBufferRingBuffer(
          v8::platform::tracing::TraceBuffer::kRingBufferChunks,
          new TraceWriter(this));
  v8_tracing_->Initialize(trace_buffer);
}

void V8cTracingController::StartTracing(
    const std::vector<std::string>& categories,
    ScriptDebugger::TraceDelegate* trace_delegate) {
  trace_delegate_ = trace_delegate;
  v8::platform::tracing::TraceConfig* trace_config =
      v8::platform::tracing::TraceConfig::CreateDefaultTraceConfig();
  for (auto it = categories.begin(); it != categories.end(); ++it) {
    trace_config->AddIncludedCategory(it->c_str());
  }
  v8_tracing_->StartTracing(trace_config);
}

void V8cTracingController::StopTracing() {
  v8_tracing_->StopTracing();
  trace_delegate_ = nullptr;
}

void V8cTracingController::TraceWriter::AppendTraceEvent(
    v8::platform::tracing::TraceObject* trace_event) {
  ScriptDebugger::TraceDelegate* delegate = controller_->GetTraceDelegate();
  if (!delegate) {
    return;
  }

  // Construct a new |JSONTraceWriter| to generate JSON of one event at a time.
  // This obviates the need to re-implement the trace event format here.
  // https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU
  std::stringstream json_stream;
  std::unique_ptr<v8::platform::tracing::TraceWriter> json_writer(
      v8::platform::tracing::TraceWriter::CreateJSONTraceWriter(json_stream));

  // We only want one event for the delegate, so strip the "{traceEvents:["
  // header that the |JSONTraceWriter| constructor put into the stream.
  json_stream.str(std::string());

  // Write this one event to the stream and pass the JSON to the delegate. The
  // |JSONTraceWriter| hasn't written the "]}" closing to the stream yet since
  // its destructor hasn't run.
  json_writer->AppendTraceEvent(trace_event);
  delegate->AppendTraceEvent(json_stream.str());
}

void V8cTracingController::TraceWriter::Flush() {
  ScriptDebugger::TraceDelegate* delegate = controller_->GetTraceDelegate();
  if (delegate) {
    delegate->FlushTraceEvents();
  }
}

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

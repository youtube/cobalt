/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/trace_event/scoped_event_parser_trace.h"

#include "base/debug/trace_event_impl.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/trace_event/event_parser.h"
#include "cobalt/trace_event/json_file_outputter.h"

namespace cobalt {
namespace trace_event {

ScopedEventParserTrace::ScopedEventParserTrace(
    const EventParser::ScopedEventFlowEndCallback& flow_end_event_callback) {
  CommonInit(flow_end_event_callback);
}

ScopedEventParserTrace::ScopedEventParserTrace(
    const EventParser::ScopedEventFlowEndCallback& flow_end_event_callback,
    const FilePath& log_relative_output_path) {
  FilePath absolute_output_path;
  PathService::Get(cobalt::paths::DIR_COBALT_DEBUG_OUT, &absolute_output_path);
  absolute_output_path_ = absolute_output_path.Append(log_relative_output_path);

  CommonInit(flow_end_event_callback);
}

void ScopedEventParserTrace::CommonInit(
    const EventParser::ScopedEventFlowEndCallback& flow_end_event_callback) {
  flow_end_event_callback_ = flow_end_event_callback;

  DCHECK(!base::debug::TraceLog::GetInstance()->IsEnabled());
  base::debug::TraceLog::GetInstance()->SetEnabled(true);
}

ScopedEventParserTrace::~ScopedEventParserTrace() {
  base::debug::TraceLog* trace_log = base::debug::TraceLog::GetInstance();

  trace_log->SetEnabled(false);

  // Setup our raw TraceLog::TraceEvent callback.
  EventParser event_parser(flow_end_event_callback_);

  // Setup the JSON outputter callback if the client had provided a JSON
  // file output path.
  base::optional<JSONFileOutputter> json_outputter;
  base::debug::TraceLog::OutputCallback json_output_callback;
  if (absolute_output_path_) {
    json_outputter.emplace(*absolute_output_path_);
    if (json_outputter->GetError()) {
      DLOG(WARNING) << "Error opening JSON tracing output file for writing: "
                    << absolute_output_path_->value();
      json_outputter = base::nullopt;
    } else {
      json_output_callback =
          base::Bind(&JSONFileOutputter::OutputTraceData,
                     base::Unretained(&json_outputter.value()));
    }
  }

  // Flush the trace log through our callbacks.
  trace_log->FlushWithRawEvents(
      base::Bind(&EventParser::ParseEvent, base::Unretained(&event_parser)),
      json_output_callback);
}

namespace {
void EndEventParserTrace(scoped_ptr<ScopedEventParserTrace> trace,
                         base::Closure callback) {
  trace.reset();
  callback.Run();
}
}  // namespace

void TraceWithEventParserForDuration(
    const EventParser::ScopedEventFlowEndCallback& flow_end_event_callback,
    const base::Closure& flow_end_event_finish_callback,
    const base::TimeDelta& duration) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&EndEventParserTrace,
                 base::Passed(make_scoped_ptr(
                     new ScopedEventParserTrace(flow_end_event_callback))),
                 flow_end_event_finish_callback),
      duration);
}

bool IsTracing() { return base::debug::TraceLog::GetInstance()->IsEnabled(); }

}  // namespace trace_event
}  // namespace cobalt

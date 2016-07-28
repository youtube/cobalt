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

#ifndef COBALT_TRACE_EVENT_SCOPED_EVENT_PARSER_TRACE_H_
#define COBALT_TRACE_EVENT_SCOPED_EVENT_PARSER_TRACE_H_

#include "base/file_path.h"
#include "base/time.h"
#include "cobalt/trace_event/event_parser.h"

namespace cobalt {
namespace trace_event {

// This class will enable tracing for its lifetime and direct all trace
// results through a trace event parser which will then call the passed
// in event handling callback with the parsed trace events.
class ScopedEventParserTrace {
 public:
  explicit ScopedEventParserTrace(
      const EventParser::ScopedEventFlowEndCallback& flow_end_event_callback);
  // Optionally, a file path can also be provided in which case JSON output
  // of the trace will be written to that file.
  ScopedEventParserTrace(
      const EventParser::ScopedEventFlowEndCallback& flow_end_event_callback,
      const FilePath& log_relative_output_path);
  ~ScopedEventParserTrace();

 private:
  void CommonInit(
      const EventParser::ScopedEventFlowEndCallback& flow_end_event_callback);

  EventParser::ScopedEventFlowEndCallback flow_end_event_callback_;
  base::optional<FilePath> absolute_output_path_;
};

// Helper function to make it easy to start a trace and collect results even
// if the scope is hard to define (for example if it is difficult to quit
// from the application).
// This function will start a trace when it is called, and after the specified
// amount of time has passed, it will stop tracing and call the callback with
// the recorded events.
void TraceWithEventParserForDuration(
    const EventParser::ScopedEventFlowEndCallback& flow_end_event_callback,
    const base::Closure& flow_end_event_finish_callback,
    const base::TimeDelta& duration);

// Returns whether a trace is started and active right now.
bool IsTracing();

}  // namespace trace_event
}  // namespace cobalt

#endif  // COBALT_TRACE_EVENT_SCOPED_EVENT_PARSER_TRACE_H_

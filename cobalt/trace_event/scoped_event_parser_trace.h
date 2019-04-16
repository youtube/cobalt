// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_TRACE_EVENT_SCOPED_EVENT_PARSER_TRACE_H_
#define COBALT_TRACE_EVENT_SCOPED_EVENT_PARSER_TRACE_H_

#include "base/files/file_path.h"
#include "base/optional.h"
#include "base/time/time.h"

namespace cobalt {
namespace trace_event {

// This class will enable tracing for its lifetime and direct all trace
// results through a trace event parser which will then call the passed
// in event handling callback with the parsed trace events.
class ScopedEventParserTrace {
 public:
  explicit ScopedEventParserTrace();
  // Optionally, a file path can also be provided in which case JSON output
  // of the trace will be written to that file.
  ScopedEventParserTrace(const base::FilePath& log_relative_output_path);
  ~ScopedEventParserTrace();

 private:
  void CommonInit();

  base::Optional<base::FilePath> absolute_output_path_;
};

// Returns whether a trace is started and active right now.
bool IsTracing();

}  // namespace trace_event
}  // namespace cobalt

#endif  // COBALT_TRACE_EVENT_SCOPED_EVENT_PARSER_TRACE_H_

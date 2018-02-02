// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_TRACE_EVENT_SCOPED_TRACE_TO_FILE_H_
#define COBALT_TRACE_EVENT_SCOPED_TRACE_TO_FILE_H_

#include "base/file_path.h"
#include "base/time.h"

namespace cobalt {
namespace trace_event {

// When this object is constructed, it will enable base::debug::TraceLog (which
// should NOT already be enabled), thus enabling the recording of all
// TRACE_EVENT annotations.  Finally, when the object is destructed, the output
// will be written to the specified file in JSON format so that it can
// subsequently be analyzed by navigating to "about:tracing" in a Chrome
// browser and then clicking the "Load" button and finally navigating to the
// file that was output.
class ScopedTraceToFile {
 public:
  explicit ScopedTraceToFile(const FilePath& output_path_relative_to_logs);
  ~ScopedTraceToFile();

  const FilePath& absolute_output_path() const { return absolute_output_path_; }

 private:
  FilePath absolute_output_path_;
};

// Helper function to make it easy to start a trace and collect results even
// if the scope is hard to define (for example if it is difficult to quit
// from the application).
// This function will start a trace when it is called, and after the specified
// amount of time has passed, it will stop tracing and output the results to
// the specified file.
void TraceToFileForDuration(const FilePath& output_path_relative_to_logs,
                            const base::TimeDelta& duration);

}  // namespace trace_event
}  // namespace cobalt

#endif  // COBALT_TRACE_EVENT_SCOPED_TRACE_TO_FILE_H_

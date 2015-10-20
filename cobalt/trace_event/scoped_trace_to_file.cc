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

#include "cobalt/trace_event/scoped_trace_to_file.h"

#include "base/bind.h"
#include "base/debug/trace_event_impl.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/trace_event/json_file_outputter.h"

namespace cobalt {
namespace trace_event {

ScopedTraceToFile::ScopedTraceToFile(
    const FilePath& output_path_relative_to_logs) {
  PathService::Get(cobalt::paths::DIR_COBALT_DEBUG_OUT, &absolute_output_path_);
  absolute_output_path_ =
      absolute_output_path_.Append(output_path_relative_to_logs);

  DCHECK(!base::debug::TraceLog::GetInstance()->IsEnabled());
  base::debug::TraceLog::GetInstance()->SetEnabled(true);
}

ScopedTraceToFile::~ScopedTraceToFile() {
  base::debug::TraceLog* trace_log = base::debug::TraceLog::GetInstance();

  trace_log->SetEnabled(false);

  // Output results to absolute_output_path_.
  JSONFileOutputter outputter(absolute_output_path_);
  if (!outputter.GetError()) {
    // Write out the actual data by calling Flush().  Within Flush(), this
    // will call OutputTraceData(), possibly multiple times.
    trace_log->Flush(base::Bind(&JSONFileOutputter::OutputTraceData,
                                base::Unretained(&outputter)));
  } else {
    DLOG(WARNING) << "Error opening JSON tracing output file for writing: "
                  << absolute_output_path_.value();
  }
}

namespace {
void EndTimedTrace(scoped_ptr<ScopedTraceToFile> trace) {
  LOG(INFO) << "Timed trace ended.";
  trace.reset();
}
}  // namespace

void TraceToFileForDuration(const FilePath& output_path_relative_to_logs,
                            const base::TimeDelta& duration) {
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(
          &EndTimedTrace,
          base::Passed(make_scoped_ptr(
              new ScopedTraceToFile(output_path_relative_to_logs)))),
      duration);
}

}  // namespace trace_event
}  // namespace cobalt

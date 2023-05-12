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

#include "cobalt/trace_event/scoped_trace_to_file.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/trace_event/json_file_outputter.h"

namespace cobalt {
namespace trace_event {

// static
base::FilePath ScopedTraceToFile::filepath_to_absolute(
    const base::FilePath& output_path_relative_to_logs) {
  base::FilePath result;
  base::PathService::Get(cobalt::paths::DIR_COBALT_DEBUG_OUT, &result);
  return result.Append(output_path_relative_to_logs);
}

ScopedTraceToFile::ScopedTraceToFile(
    const base::FilePath& output_path_relative_to_logs) {
  absolute_output_path_ = filepath_to_absolute(output_path_relative_to_logs);

  DCHECK(!base::trace_event::TraceLog::GetInstance()->IsEnabled());
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig(),
      base::trace_event::TraceLog::RECORDING_MODE);
}

ScopedTraceToFile::~ScopedTraceToFile() {
  base::trace_event::TraceLog* trace_log =
      base::trace_event::TraceLog::GetInstance();

  trace_log->SetDisabled(base::trace_event::TraceLog::RECORDING_MODE);

  // Output results to absolute_output_path_.
  JSONFileOutputter outputter(absolute_output_path_);
  if (outputter.Output(trace_log)) {
    LOG(INFO) << "Trace output written to " << absolute_output_path_.value();
  } else {
    LOG(WARNING) << "Error opening JSON tracing output file for writing: "
                 << absolute_output_path_.value();
  }
}

namespace {
void EndTimedTrace(std::unique_ptr<ScopedTraceToFile> trace) {
  LOG(INFO) << "Timed trace ended.";
  trace.reset();
}
}  // namespace

void TraceToFileForDuration(const base::FilePath& output_path_relative_to_logs,
                            const base::TimeDelta& duration) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&EndTimedTrace,
                 base::Passed(base::WrapUnique(
                     new ScopedTraceToFile(output_path_relative_to_logs)))),
      duration);
}

}  // namespace trace_event
}  // namespace cobalt

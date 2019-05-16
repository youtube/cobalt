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

#include <memory>

#include "cobalt/trace_event/scoped_event_parser_trace.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_impl.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/trace_event/json_file_outputter.h"

namespace cobalt {
namespace trace_event {

ScopedEventParserTrace::ScopedEventParserTrace() { CommonInit(); }

ScopedEventParserTrace::ScopedEventParserTrace(
    const base::FilePath& log_relative_output_path) {
  base::FilePath absolute_output_path;
  base::PathService::Get(cobalt::paths::DIR_COBALT_DEBUG_OUT,
                         &absolute_output_path);
  absolute_output_path_ = absolute_output_path.Append(log_relative_output_path);

  CommonInit();
}

void ScopedEventParserTrace::CommonInit() {
  DCHECK(!base::trace_event::TraceLog::GetInstance()->IsEnabled());
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig(
          "", "record-as-much-as-possible, enable-systrace"),
      base::trace_event::TraceLog::RECORDING_MODE);
  DCHECK(base::trace_event::TraceLog::GetInstance()->IsEnabled());
}

ScopedEventParserTrace::~ScopedEventParserTrace() {
  base::trace_event::TraceLog* trace_log =
      base::trace_event::TraceLog::GetInstance();

  trace_log->SetDisabled();

  // Setup the JSON outputter callback if the client had provided a JSON
  // file output path.
  base::Optional<JSONFileOutputter> json_outputter;
  base::trace_event::TraceLog::OutputCallback json_output_callback =
      base::Bind([](const scoped_refptr<base::RefCountedString>&, bool) {});
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
  trace_log->Flush(json_output_callback);
}

bool IsTracing() {
  return base::trace_event::TraceLog::GetInstance()->IsEnabled();
}

}  // namespace trace_event
}  // namespace cobalt

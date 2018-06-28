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

#include "cobalt/trace_event/json_file_outputter.h"

#include <string>

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
#include "base/command_line.h"
#endif
#include "base/logging.h"
#include "base/platform_file.h"
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
#include "base/string_piece.h"
#include "cobalt/trace_event/switches.h"
#endif

namespace cobalt {
namespace trace_event {

// Returns true if and only if log_timed_trace == "on", and
// ENABLE_DEBUG_COMMAND_LINE_SWITCHES is set.
bool ShouldLogTimedTrace() {
  bool isTimedTraceSet = false;

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kLogTimedTrace) &&
      command_line->GetSwitchValueASCII(switches::kLogTimedTrace) ==
          "on") {
    isTimedTraceSet = true;
  }

#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  return isTimedTraceSet;
}

JSONFileOutputter::JSONFileOutputter(const FilePath& output_path)
    : output_path_(output_path),
      output_trace_event_call_count_(0),
      file_(base::kInvalidPlatformFileValue) {
  file_ = base::CreatePlatformFile(
      output_path,
      base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE, NULL,
      NULL);
  if (GetError()) {
    DLOG(ERROR) << "Unable to open file for writing: " << output_path.value();
  } else {
    std::string header_text("{\"traceEvents\": [\n");
    Write(header_text.c_str(), header_text.size());
  }
}

JSONFileOutputter::~JSONFileOutputter() {
  if (GetError()) {
    DLOG(ERROR) << "An error occurred while writing to the output file: "
                << output_path_.value();
  } else {
    const char tail[] = "\n]}";
    Write(tail, strlen(tail));
  }
  Close();
}

void JSONFileOutputter::OutputTraceData(
    const scoped_refptr<base::RefCountedString>& event_string) {
  DCHECK(!GetError());

  // This function is usually called as a callback by
  // base::debug::TraceLog::Flush().  It may get called by Flush()
  // multiple times.
  if (output_trace_event_call_count_ != 0) {
    const char text[] = ",\n";
    Write(text, strlen(text));
  }
  const std::string& event_str = event_string->data();
  Write(event_str.c_str(), event_str.size());
  ++output_trace_event_call_count_;
}

void JSONFileOutputter::Write(const char* buffer, size_t length) {
  if (GetError()) {
    return;
  }

  if (ShouldLogTimedTrace()) {
    // These markers assist in locating the trace data in the log, and can be
    // used by scripts to help extract the trace data.
    LOG(INFO) << "BEGIN_TRACELOG_MARKER" << base::StringPiece(buffer, length)
              << "END_TRACELOG_MARKER";
    return;
  }

  int count = base::WritePlatformFileAtCurrentPos(file_, buffer, length);
  if (count < 0) {
    Close();
  }
}

void JSONFileOutputter::Close() {
  if (GetError()) {
    return;
  }

  base::ClosePlatformFile(file_);
  file_ = base::kInvalidPlatformFileValue;
}

}  // namespace trace_event
}  // namespace cobalt

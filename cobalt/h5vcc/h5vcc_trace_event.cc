// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_trace_event.h"

#include "base/files/file_util.h"

namespace cobalt {
namespace h5vcc {

namespace {
const char* kOutputTraceFilename = "h5vcc_trace_event.json";
}  // namespace

H5vccTraceEvent::H5vccTraceEvent() {}

void H5vccTraceEvent::Start(const std::string& output_filename) {
  if (trace_to_file_) {
    DLOG(WARNING) << "H5vccTraceEvent is already started.";
  } else {
    base::FilePath output_filepath(
        output_filename.empty() ? kOutputTraceFilename : output_filename);
    last_absolute_path_.clear();
    trace_to_file_.reset(new trace_event::ScopedTraceToFile(output_filepath));
  }
}

void H5vccTraceEvent::Stop() {
  if (trace_to_file_) {
    last_absolute_path_ = trace_to_file_->absolute_output_path();
    trace_to_file_.reset();
  } else {
    DLOG(WARNING) << "H5vccTraceEvent is already stopped.";
  }
}

std::string H5vccTraceEvent::Read(const std::string& read_filename) {
  if (trace_to_file_) {
    Stop();
  }
  std::string trace;
  if (!read_filename.empty()) {
    auto path = trace_event::ScopedTraceToFile::filepath_to_absolute(
        base::FilePath(read_filename));
    ReadFileToString(path, &trace);
  } else if (!last_absolute_path_.empty()) {
    ReadFileToString(last_absolute_path_, &trace);
  }
  return trace;
}


}  // namespace h5vcc
}  // namespace cobalt

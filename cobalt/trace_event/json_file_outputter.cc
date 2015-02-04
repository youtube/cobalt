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

#include "cobalt/trace_event/json_file_outputter.h"

#include <string>

#include "base/file_util.h"
#include "base/logging.h"

namespace cobalt {
namespace trace_event {

JSONFileOutputter::JSONFileOutputter(const FilePath& output_path)
    : output_path_(output_path), output_trace_event_call_count_(0),
      fp_(file_util::OpenFile(output_path, "w")) {
  if (!fp_) {
    DLOG(ERROR) << "Unable to open file for writing: " << output_path.value();
  } else {
    std::string header_text("{\"traceEvents\": [\n");
    fwrite(header_text.c_str(), sizeof(header_text[0]), header_text.size(),
           fp_.get());
  }
}

JSONFileOutputter::~JSONFileOutputter() {
  if (fp_) {
    if (GetError()) {
      DLOG(ERROR) << "An error occurred while writing to the output file: "
                  << output_path_.value();
    } else {
      fprintf(fp_.get(), "\n]}");
    }
  }
}

void JSONFileOutputter::OutputTraceData(
    const scoped_refptr<base::RefCountedString>& event_string) {
  DCHECK(!GetError());

  // This function is usually called as a callback by
  // base::debug::TraceLog::Flush().  It may get called by Flush()
  // multiple times.
  if (output_trace_event_call_count_ != 0) {
    fprintf(fp_.get(), ",\n");
  }
  const std::string& event_str = event_string->data();
  fwrite(event_str.c_str(), sizeof(event_str[0]), event_str.size(), fp_.get());

  ++output_trace_event_call_count_;
}

}  // namespace trace_event
}  // namespace cobalt

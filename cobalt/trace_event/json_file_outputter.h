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

#ifndef COBALT_TRACE_EVENT_JSON_FILE_OUTPUTTER_H_
#define COBALT_TRACE_EVENT_JSON_FILE_OUTPUTTER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/platform_file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/trace_event/trace_log.h"

namespace cobalt {
namespace trace_event {

// This class is a helper class to provide a callback for
// base::trace_event::TraceLog::Flush() calls so that the JSON produced by
// TraceLog can be directed to a file on disc.
class JSONFileOutputter {
 public:
  explicit JSONFileOutputter(const base::FilePath& output_path);
  ~JSONFileOutputter();

  // Write all content held inside |trace_log| to |file_|.  Returns true on
  // success, otherwise returns false.
  bool Output(base::trace_event::TraceLog* trace_log);

  bool GetError() const { return file_ == base::kInvalidPlatformFile; }

 private:
  void OutputTraceData(
      base::OnceClosure finished_cb,
      const scoped_refptr<base::RefCountedString>& event_string,
      bool has_more_events);
  void Write(const char* buffer, int length);
  void Close();

  base::FilePath output_path_;
  base::PlatformFile file_;

  int output_trace_event_call_count_;
};

}  // namespace trace_event
}  // namespace cobalt

#endif  // COBALT_TRACE_EVENT_JSON_FILE_OUTPUTTER_H_

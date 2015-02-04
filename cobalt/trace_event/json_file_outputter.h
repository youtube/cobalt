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

#ifndef TRACE_EVENT_JSON_FILE_OUTPUTTER_H_
#define TRACE_EVENT_JSON_FILE_OUTPUTTER_H_

#include <cstdio>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"

namespace cobalt {
namespace trace_event {

// This class is a helper class to provide a callback for
// base::debug::TraceLog::Flush() calls so that the JSON produced by TraceLog
// can be directed to a file on disc.
class JSONFileOutputter {
 public:
  explicit JSONFileOutputter(const FilePath& output_path);
  ~JSONFileOutputter();

  void OutputTraceData(
      const scoped_refptr<base::RefCountedString>& event_string);

  bool GetError() const { return !fp_ || ferror(fp_.get()) != 0; }

 private:
  FilePath output_path_;
  file_util::ScopedFILE fp_;

  int output_trace_event_call_count_;
};

}  // namespace trace_event
}  // namespace cobalt

#endif  // TRACE_EVENT_JSON_FILE_OUTPUTTER_H_

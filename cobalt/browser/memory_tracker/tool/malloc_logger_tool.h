// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_MALLOC_LOGGER_TOOL_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_MALLOC_LOGGER_TOOL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/time.h"
#include "cobalt/browser/memory_tracker/tool/params.h"
#include "cobalt/browser/memory_tracker/tool/tool_impl.h"
#include "nb/memory_scope.h"
#include "starboard/atomic.h"
#include "starboard/memory_reporter.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

class BufferedFileWriter;

// Outputs memory_log_<time_stamp>.csv to the output log location. This log
// contains allocations and deallocations with a non-symbolized stack trace.
class MallocLoggerTool: public AbstractTool,
  public nb::analytics::MemoryTrackerDebugCallback {
 public:
  MallocLoggerTool();
  virtual ~MallocLoggerTool();

  // Interface AbstractMemoryTrackerTool
  std::string tool_name() const override;
  void Run(Params* params) override;

  // OnMemoryAllocation() and OnMemoryDeallocation() are part of
  // class MemoryTrackerDebugCallback.
  void OnMemoryAllocation(const void* memory_block,
                          const nb::analytics::AllocationRecord& record,
                          const nb::analytics::CallStack& callstack) override;

  void OnMemoryDeallocation(const void* memory_block,
                            const nb::analytics::AllocationRecord& record,
                            const nb::analytics::CallStack& callstack) override;

  // Method to obtain allocation, stack information and generate records
  void LogRecord(const void* memory_block,
    const nb::analytics::AllocationRecord& record,
    const nb::analytics::CallStack& callstack,
    const int type);

 private:
  static std::string MemoryLogPath();
  static base::TimeTicks NowTime();

  int GetTimeSinceStartMs() const;

  base::TimeTicks start_time_;
  scoped_ptr<SbMemoryReporter> memory_reporter_;
  scoped_ptr<BufferedFileWriter> buffered_file_writer_;
  starboard::atomic_int32_t atomic_counter_;
  starboard::atomic_int64_t atomic_used_memory_;
};

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_MALLOC_LOGGER_TOOL_H_

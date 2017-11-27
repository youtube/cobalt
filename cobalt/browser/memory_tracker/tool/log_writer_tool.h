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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_LOG_WRITER_TOOL_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_LOG_WRITER_TOOL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/time.h"
#include "cobalt/browser/memory_tracker/tool/params.h"
#include "cobalt/browser/memory_tracker/tool/tool_impl.h"
#include "starboard/memory_reporter.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

class BufferedFileWriter;

// Outputs memory_log.txt to the output log location. This log contains
// allocations with a stack trace (non-symbolized) and deallocations without
// a stack.
class LogWriterTool : public AbstractTool {
 public:
  LogWriterTool();
  virtual ~LogWriterTool();

  // Interface AbstrctMemoryTrackerTool
  std::string tool_name() const override;
  void Run(Params* params) override;

  void OnMemoryAllocation(const void* memory_block, size_t size);
  void OnMemoryDeallocation(const void* memory_block);

 private:
  // Callbacks for MemoryReporter
  static void OnAlloc(void* context, const void* memory, size_t size);
  static void OnDealloc(void* context, const void* memory);
  static void OnMapMemory(void* context, const void* memory, size_t size);
  static void OnUnMapMemory(void* context, const void* memory, size_t size);
  static std::string MemoryLogPath();

  static base::TimeTicks NowTime();
  static const size_t kStartIndex = 5;
  static const size_t kNumAddressPrints = 1;
  static const size_t kMaxStackSize = 10;

  int GetTimeSinceStartMs() const;
  void InitAndRegisterMemoryReporter();

  base::TimeTicks start_time_;
  scoped_ptr<SbMemoryReporter> memory_reporter_;
  scoped_ptr<BufferedFileWriter> buffered_file_writer_;
};

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_LOG_WRITER_TOOL_H_

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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_PRINT_CSV_TOOL_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_PRINT_CSV_TOOL_H_

#include <map>
#include <string>

#include "base/compiler_specific.h"
#include "cobalt/browser/memory_tracker/tool/params.h"
#include "cobalt/browser/memory_tracker/tool/tool_impl.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

// Generates CSV values of the engine.
// There are three sections of data including:
//   1. average bytes / alloc
//   2. # Bytes allocated per memory scope.
//   3. # Allocations per memory scope.
// This data can be pasted directly into a Google spreadsheet and visualized.
// Note that this thread will implicitly call Start() is called during
// construction and Cancel() & Join() during destruction.
class PrintCSVTool : public AbstractTool {
 public:
  // This tool will only produce on CSV dump of the engine. This is useful
  // for profiling startup memory consumption.
  PrintCSVTool(int sampling_interval_ms, int sampling_time_ms);

  // Overridden so that the thread can exit gracefully.
  void Run(Params* params) override;
  std::string tool_name() const override { return "MemoryTrackerPrintCSV"; }

 private:
  typedef std::map<std::string, AllocationSamples> MapAllocationSamples;
  static std::string ToCsvString(const MapAllocationSamples& samples);
  static const char* UntrackedMemoryKey();
  bool TimeExpiredYet(const Params& params);

  const int sample_interval_ms_;
  const int sampling_time_ms_;
};

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_PRINT_CSV_TOOL_H_

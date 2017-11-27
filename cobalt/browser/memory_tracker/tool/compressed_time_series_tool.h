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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_COMPRESSED_TIME_SERIES_TOOL_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_COMPRESSED_TIME_SERIES_TOOL_H_

#include <string>

#include "cobalt/browser/memory_tracker/tool/tool_impl.h"
#include "cobalt/browser/memory_tracker/tool/util.h"
#include "nb/analytics/memory_tracker.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

// Generates CSV values of the engine memory. This includes memory
// consumption in bytes and also the number of allocations.
// Allocations are segmented by category such as "Javascript" and "Network and
// are periodically dumped to the Params::abstract_logger_.
//
// Compression:
//  During early run, the sampling rate is extremely high, however the sampling
//  rate will degrade by half each time the sampling buffer fills up.
//
//  Additionally, every time the sampling buffer fills up, space is freed by
//  evicting every other element.
//
// Example output:
//  //////////////////////////////////////////////
//  // CSV of BYTES allocated per region (MB's).
//  // Units are in Megabytes. This is designed
//  // to be used in a stacked graph.
//  "Time(mins)","CSS","DOM","Javascript","Media","MessageLoop"....
//  0,0,0,0,0,0...
//  1022.54,3.3,7.19,49.93,50.33....
//  ...
//  // END CSV of BYTES allocated per region.
//  //////////////////////////////////////////////
//
//  //////////////////////////////////////////////
//  // CSV of COUNT of allocations per region.
//  "Time(mins)","CSS","DOM","Javascript","Media","MessageLoop"....
//  0,0,0,0,0,0...
//  1022.54,57458,70011,423217,27,54495,43460...
//  ...
//  // END CSV of COUNT of allocations per region.
//  //////////////////////////////////////////////
class CompressedTimeSeriesTool : public AbstractTool {
 public:
  CompressedTimeSeriesTool();
  void Run(Params* params) override;
  std::string tool_name() const override {
    return "MemoryTrackerCompressedTimeSeries";
  }

 private:
  static std::string ToCsvString(const TimeSeries& histogram);
  static void AcquireSample(nb::analytics::MemoryTracker* memory_tracker,
                            TimeSeries* histogram, const base::TimeDelta& dt);
  static bool IsFull(const TimeSeries& histogram, size_t num_samples);
  static void Compress(TimeSeries* histogram);

  const int number_samples_;
};

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_COMPRESSED_TIME_SERIES_TOOL_H_

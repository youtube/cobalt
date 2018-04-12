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

#include "cobalt/browser/memory_tracker/tool/malloc_stats_tool.h"

#include <string>
#include <vector>

#include "cobalt/browser/memory_tracker/tool/histogram_table_csv_base.h"
#include "cobalt/browser/memory_tracker/tool/params.h"
#include "cobalt/browser/memory_tracker/tool/tool_impl.h"
#include "nb/analytics/memory_tracker.h"
#include "starboard/string.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {
namespace {

// Takes in bytes -> outputs as megabytes.
class MemoryBytesHistogramCSV : public HistogramTableCSVBase<int64_t> {
 public:
  MemoryBytesHistogramCSV() : HistogramTableCSVBase<int64_t>(0) {}
  std::string ValueToString(const int64_t& bytes) const override {
    return ToMegabyteString(bytes);
  }

  static std::string ToMegabyteString(int64_t bytes) {
    double mega_bytes = static_cast<double>(bytes) / (1024.0 * 1024.0);
    char buff[128];
    SbStringFormatF(buff, sizeof(buff), "%.1f", mega_bytes);
    return std::string(buff);
  }
};

}  // namespace.

MallocStatsTool::MallocStatsTool() {
}

std::string MallocStatsTool::tool_name() const  {
  return "MallocStatsTool";
}

void MallocStatsTool::Run(Params* params) {
  params->logger()->Output("MallocStatsTool running...\n");

  Timer output_timer(base::TimeDelta::FromSeconds(30));
  Timer sample_timer(base::TimeDelta::FromMilliseconds(50));

  MemoryBytesHistogramCSV histogram_table;
  histogram_table.set_title("Malloc Stats");

  // If we get a finish signal then this will break out of the loop.
  while (!params->wait_for_finish_signal(250 * kSbTimeMillisecond)) {
    // LOG CSV.
    if (output_timer.UpdateAndIsExpired()) {
      std::stringstream ss;
      ss << kNewLine << histogram_table.ToString() << kNewLine << kNewLine;
      params->logger()->Output(ss.str());
    }

    // ADD A HISTOGRAM SAMPLE.
    if (sample_timer.UpdateAndIsExpired()) {
      // Take a sample.
      nb::analytics::MemoryStats memory_stats =
          nb::analytics::GetProcessMemoryStats();

      histogram_table.BeginRow(params->time_since_start());
      histogram_table.AddRowValue(
          "TotalCpuMemory(MB)", memory_stats.total_cpu_memory);
      histogram_table.AddRowValue(
          "UsedCpuMemory(MB)", memory_stats.used_cpu_memory);
      histogram_table.AddRowValue(
          "TotalGpuMemory(MB)", memory_stats.total_gpu_memory);
      histogram_table.AddRowValue(
          "UsedGpuMemory(MB)", memory_stats.used_gpu_memory);
      histogram_table.FinalizeRow();
    }

    // COMPRESS TABLE WHEN FULL.
    //
    // Table is full, therefore eliminate half of the elements.
    // Reduce sample frequency to match.
    if (histogram_table.NumberOfRows() >= 100) {
      // Compression step.
      histogram_table.RemoveOddElements();

      // By double the sampling time this keeps the table linear with
      // respect to time. If sampling time was not doubled then there
      // would be time distortion in the graph.
      sample_timer.ScaleTriggerTime(2.0);
      sample_timer.Restart();
    }
  }
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

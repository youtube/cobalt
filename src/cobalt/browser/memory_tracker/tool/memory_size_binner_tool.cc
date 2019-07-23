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

#include "cobalt/browser/memory_tracker/tool/memory_size_binner_tool.h"

#include <sstream>
#include <string>
#include <vector>

#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "cobalt/base/c_val.h"
#include "cobalt/browser/memory_tracker/tool/util.h"
#include "nb/analytics/memory_tracker.h"
#include "nb/analytics/memory_tracker_helpers.h"
#include "starboard/common/log.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {
namespace {

const nb::analytics::AllocationGroup* FindAllocationGroup(
    const std::string& name, nb::analytics::MemoryTracker* memory_tracker) {
  std::vector<const nb::analytics::AllocationGroup*> groups;
  memory_tracker->GetAllocationGroups(&groups);
  // Find by exact string match.
  for (size_t i = 0; i < groups.size(); ++i) {
    const nb::analytics::AllocationGroup* group = groups[i];
    if (group->name().compare(name) == 0) {
      return group;
    }
  }
  return NULL;
}
}  // namespace.

MemorySizeBinnerTool::MemorySizeBinnerTool(const std::string& memory_scope_name)
    : memory_scope_name_(memory_scope_name) {}

void MemorySizeBinnerTool::Run(Params* params) {
  const nb::analytics::AllocationGroup* target_group = NULL;

  while (!params->finished()) {
    if (target_group == NULL && !memory_scope_name_.empty()) {
      target_group =
          FindAllocationGroup(memory_scope_name_, params->memory_tracker());
    }

    std::stringstream ss;
    ss.precision(2);
    if (target_group || memory_scope_name_.empty()) {
      AllocationSizeBinner visitor_binner = AllocationSizeBinner(target_group);
      params->memory_tracker()->Accept(&visitor_binner);

      size_t min_size = 0;
      size_t max_size = 0;

      visitor_binner.GetLargestSizeRange(&min_size, &max_size);

      FindTopSizes top_size_visitor =
          FindTopSizes(min_size, max_size, target_group);
      params->memory_tracker()->Accept(&top_size_visitor);

      ss << kNewLine;
      ss << "TimeNow " << params->TimeInMinutesString() << " (minutes):";
      ss << kNewLine;
      if (!memory_scope_name_.empty()) {
        ss << "Tracking Memory Scope \"" << memory_scope_name_ << "\", ";
      } else {
        ss << "Tracking whole program, ";
      }
      ss << "first row is allocation size range, second row is number of "
         << kNewLine << "allocations in that range." << kNewLine;
      ss << visitor_binner.ToCSVString();
      ss << kNewLine;
      ss << "Largest allocation range: \"" << min_size << "..." << max_size
         << "\"" << kNewLine;
      ss << "Printing out top allocations from this range: " << kNewLine;
      ss << top_size_visitor.ToString(5) << kNewLine;
    } else {
      ss << "No allocations for \"" << memory_scope_name_ << "\".";
    }

    params->logger()->Output(ss.str().c_str());
    params->logger()->Flush();

    // Sleep until the next sample.
    base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(1));
  }
}

std::string MemorySizeBinnerTool::tool_name() const {
  return "MemoryTrackerCompressedTimeSeries";
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

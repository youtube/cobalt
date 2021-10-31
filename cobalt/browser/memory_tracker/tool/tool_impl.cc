// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/memory_tracker/tool/tool_impl.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "cobalt/base/c_val_collection_entry_stats.h"
#include "cobalt/browser/memory_tracker/tool/params.h"
#include "cobalt/browser/memory_tracker/tool/tool_thread.h"
#include "cobalt/browser/memory_tracker/tool/util.h"
#include "nb/analytics/memory_tracker.h"
#include "nb/analytics/memory_tracker_helpers.h"
#include "nb/concurrent_map.h"
#include "nb/memory_scope.h"
#include "starboard/common/semaphore.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/file.h"
#include "starboard/system.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

using nb::analytics::AllocationGroup;
using nb::analytics::AllocationRecord;
using nb::analytics::AllocationVisitor;
using nb::analytics::GetProcessMemoryStats;
using nb::analytics::MemoryStats;
using nb::analytics::MemoryTracker;

size_t AllocationSizeBinner::GetBucketIndexForAllocationSize(size_t size) {
  for (int i = 0; i < 32; ++i) {
    size_t val = static_cast<size_t>(0x1) << i;
    if (val > size) {
      return i;
    }
  }
  SB_NOTREACHED();
  return 32;
}

void AllocationSizeBinner::GetSizeRange(size_t size, size_t* min_value,
                                        size_t* max_value) {
  size_t idx = GetBucketIndexForAllocationSize(size);
  IndexToSizeRange(idx, min_value, max_value);
}

void AllocationSizeBinner::IndexToSizeRange(size_t idx, size_t* min_value,
                                            size_t* max_value) {
  if (idx == 0) {
    *min_value = 0;
    *max_value = 0;
    return;
  }
  *min_value = static_cast<size_t>(0x1) << (idx - 1);
  *max_value = (*min_value << 1) - 1;
  return;
}

size_t AllocationSizeBinner::GetIndexRepresentingMostMemoryConsumption() const {
  int64 largest_allocation_total = 0;
  size_t largest_allocation_total_idx = 0;

  for (size_t i = 0; i < allocation_histogram_.size(); ++i) {
    size_t alloc_size = static_cast<size_t>(0x1) << i;
    size_t count = allocation_histogram_[i];
    int64 allocation_total =
        static_cast<int64>(alloc_size) * static_cast<int64>(count);

    if (largest_allocation_total < allocation_total) {
      largest_allocation_total = allocation_total;
      largest_allocation_total_idx = i;
    }
  }
  return largest_allocation_total_idx;
}

void AllocationSizeBinner::GetLargestSizeRange(size_t* min_value,
                                               size_t* max_value) const {
  size_t index = GetIndexRepresentingMostMemoryConsumption();
  IndexToSizeRange(index, min_value, max_value);
}

AllocationSizeBinner::AllocationSizeBinner(const AllocationGroup* group_filter)
    : group_filter_(group_filter) {
  allocation_histogram_.resize(33);
}

bool AllocationSizeBinner::PassesFilter(
    const AllocationRecord& alloc_record) const {
  if (group_filter_ == NULL) {
    return true;
  }

  return alloc_record.allocation_group == group_filter_;
}

bool AllocationSizeBinner::Visit(const void* memory,
                                 const AllocationRecord& alloc_record) {
  if (PassesFilter(alloc_record)) {
    const size_t idx = GetBucketIndexForAllocationSize(alloc_record.size);
    allocation_histogram_[idx]++;
  }
  return true;
}

std::string AllocationSizeBinner::ToCSVString() const {
  size_t first_idx = 0;
  size_t end_idx = allocation_histogram_.size();

  // Determine the start index by skipping all consecutive head entries
  // that are 0.
  while (first_idx < allocation_histogram_.size()) {
    const size_t num_allocs = allocation_histogram_[first_idx];
    if (num_allocs > 0) {
      break;
    }
    first_idx++;
  }

  // Determine the end index by skipping all consecutive tail entries
  // that are 0.
  while (end_idx > 0) {
    if (end_idx < allocation_histogram_.size()) {
      const size_t num_allocs = allocation_histogram_[end_idx];
      if (num_allocs > 0) {
        ++end_idx;
        break;
      }
    }
    end_idx--;
  }

  std::stringstream ss;
  for (size_t i = first_idx; i < end_idx; ++i) {
    size_t min = 0;
    size_t max = 0;
    IndexToSizeRange(i, &min, &max);
    std::stringstream name_ss;
    name_ss << kQuote << min << "..." << max << kQuote;
    ss << name_ss.str() << kDelimiter;
  }
  ss << kNewLine;

  for (size_t i = first_idx; i < end_idx; ++i) {
    const size_t num_allocs = allocation_histogram_[i];
    ss << num_allocs << kDelimiter;
  }
  ss << kNewLine;
  return ss.str();
}

FindTopSizes::FindTopSizes(size_t minimum_size, size_t maximum_size,
                           const AllocationGroup* group)
    : minimum_size_(minimum_size),
      maximum_size_(maximum_size),
      group_filter_(group) {}

bool FindTopSizes::Visit(const void* memory,
                         const AllocationRecord& alloc_record) {
  if (PassesFilter(alloc_record)) {
    size_counter_[alloc_record.size]++;
  }
  return true;
}

std::string FindTopSizes::ToString(size_t max_elements_to_print) const {
  std::vector<GroupAllocation> group_allocs = GetTopAllocations();
  const size_t n = std::min(max_elements_to_print, group_allocs.size());

  if (!group_allocs.empty()) {
    std::stringstream ss;

    for (size_t i = 0; i < n; ++i) {
      GroupAllocation g = group_allocs[i];
      size_t total_size = g.allocation_count * g.allocation_size;
      ss << "    " << total_size
         << " bytes allocated with object size: " << g.allocation_size
         << " bytes in " << g.allocation_count << " instances " << kNewLine;
    }
    return ss.str();
  } else {
    return std::string();
  }
}

std::vector<FindTopSizes::GroupAllocation> FindTopSizes::GetTopAllocations()
    const {
  std::vector<GroupAllocation> group_allocs;
  // Push objects to a vector.
  for (SizeCounterMap::const_iterator it = size_counter_.begin();
       it != size_counter_.end(); ++it) {
    GroupAllocation alloc = {it->first, it->second};
    group_allocs.push_back(alloc);
  }

  std::sort(group_allocs.begin(), group_allocs.end(),
            GroupAllocation::LessAllocationSize);
  // Biggest first.
  std::reverse(group_allocs.begin(), group_allocs.end());
  return group_allocs;
}

bool FindTopSizes::PassesFilter(const AllocationRecord& alloc_record) const {
  if (alloc_record.size < minimum_size_) return false;
  if (alloc_record.size > maximum_size_) return false;
  if (!group_filter_) return true;  // No group filter when null.
  return group_filter_ == alloc_record.allocation_group;
}

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

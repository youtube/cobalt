// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_TOOL_IMPL_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_TOOL_IMPL_H_

#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/simple_thread.h"
#include "base/time.h"
#include "cobalt/browser/memory_tracker/tool/buffered_file_writer.h"
#include "cobalt/browser/memory_tracker/tool/util.h"
#include "nb/analytics/memory_tracker.h"
#include "nb/analytics/memory_tracker_helpers.h"
#include "nb/concurrent_map.h"
#include "nb/string_interner.h"
#include "nb/thread_local_object.h"
#include "starboard/memory_reporter.h"

namespace starboard {
class ScopedFile;
}  // namespace starboard

namespace nb {
namespace analytics {
class AllocationGroup;
class AllocationRecord;
class MemoryTracker;
}  // namespace analytics
}  // namespace nb

namespace cobalt {
namespace browser {
namespace memory_tracker {

// Interface for logging. This allows dependency inject to override in the case
// of future tests.
class AbstractLogger {
 public:
  virtual ~AbstractLogger() {}
  virtual void Output(const char* str) = 0;
  void Output(const std::string& str) { Output(str.c_str()); }
  virtual void Flush() = 0;
};

// Params holds important data needed by the MemoryTracker tools during
// their run cycle.
class Params;

// Base class for all tools.
class AbstractTool {
 public:
  virtual ~AbstractTool() {}
  virtual std::string tool_name() const = 0;
  virtual void Run(Params* params) = 0;
};

// Bins the size according from all the encountered allocations.
// If AllocationGroup* is non-null, then this is used as a filter such that
// ONLY allocations belonging to that AllocationGroup are considered.
class AllocationSizeBinner : public nb::analytics::AllocationVisitor {
 public:
  typedef std::vector<int> AllocationHistogram;
  // Maps the input size to a bin number. This function performs a 2^n
  // mapping. Here is a table of some size mappings:
  // Example:
  //   GetIndex(0) == 0;
  //   GetIndex(1) == 1;
  //   GetIndex(2) == 2;
  //   GetIndex(3) == 2;
  //   GetIndex(4) == 3;
  //   ...
  //   GetIndex(15) == 4;
  //   GetIndex(16) == 5;
  static size_t GetBucketIndexForAllocationSize(size_t size);
  static void GetSizeRange(size_t size, size_t* min_value, size_t* max_value);
  static void IndexToSizeRange(size_t size, size_t* min_value,
                               size_t* max_value);

  explicit AllocationSizeBinner(
      const nb::analytics::AllocationGroup* group_filter);
  bool Visit(const void* memory,
             const nb::analytics::AllocationRecord& alloc_record) override;

  size_t GetIndexRepresentingMostMemoryConsumption() const;
  void GetLargestSizeRange(size_t* min_value, size_t* max_value) const;
  bool PassesFilter(const nb::analytics::AllocationRecord& alloc_record) const;
  // Outputs CSV formatted string of the data values.
  // The header containing the binning range is printed first, then the
  // the number of allocations in that bin.
  // Example:
  //   "16...32","32...64","64...128","128...256",...
  //   831,3726,3432,10285,...
  //
  // In this example there are 831 allocations of size 16-32 bytes.
  std::string ToCSVString() const;
  const AllocationHistogram& allocation_histogram() const {
    return allocation_histogram_;
  }

 private:
  AllocationHistogram allocation_histogram_;
  // Only these allocations are tracked.
  const nb::analytics::AllocationGroup* group_filter_;
};

// Finds the top allocations by size.
class FindTopSizes : public nb::analytics::AllocationVisitor {
 public:
  FindTopSizes(size_t minimum_size, size_t maximum_size,
               const nb::analytics::AllocationGroup* group);

  bool Visit(const void* memory,
             const nb::analytics::AllocationRecord& alloc_record) override;

  struct GroupAllocation {
    size_t allocation_size;
    size_t allocation_count;

    static bool LessAllocationSize(GroupAllocation a, GroupAllocation b) {
      size_t total_size_a = a.allocation_size * a.allocation_count;
      size_t total_size_b = b.allocation_size * b.allocation_count;
      return total_size_a < total_size_b;
    }
  };

  std::string ToString(size_t max_elements_to_print) const;
  std::vector<GroupAllocation> GetTopAllocations() const;

 private:
  typedef std::map<size_t, size_t> SizeCounterMap;

  bool PassesFilter(const nb::analytics::AllocationRecord& alloc_record) const;
  size_t minimum_size_;
  size_t maximum_size_;
  const nb::analytics::AllocationGroup* group_filter_;
  SizeCounterMap size_counter_;
};

}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_TOOL_IMPL_H_

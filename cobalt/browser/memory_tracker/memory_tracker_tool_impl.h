/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_MEMORY_TRACKER_TOOL_IMPL_H_
#define COBALT_BROWSER_MEMORY_TRACKER_MEMORY_TRACKER_TOOL_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/simple_thread.h"
#include "base/time.h"
#include "nb/analytics/memory_tracker.h"

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
  virtual void Flush() = 0;
};

// Params holds important data needed by the MemoryTracker tools during
// their run cycle.
class Params;

//
class AbstractMemoryTrackerTool {
 public:
  virtual ~AbstractMemoryTrackerTool() {}
  virtual std::string tool_name() const = 0;
  virtual void Run(Params* params) = 0;
};

class MemoryTrackerToolThread : public base::SimpleThread {
 public:
  typedef base::SimpleThread Super;
  MemoryTrackerToolThread(nb::analytics::MemoryTracker* memory_tracker,
                          AbstractMemoryTrackerTool* tool,
                          AbstractLogger* logger);
  virtual ~MemoryTrackerToolThread();

  virtual void Join() OVERRIDE;
  virtual void Run() OVERRIDE;

 private:
  scoped_ptr<Params> params_;
  scoped_ptr<AbstractMemoryTrackerTool> tool_;
};

// Start() is called when this object is created, and Cancel() & Join() are
// called during destruction.
class MemoryTrackerPrint : public AbstractMemoryTrackerTool {
 public:
  MemoryTrackerPrint() {}

  // Overridden so that the thread can exit gracefully.
  virtual void Run(Params* params) OVERRIDE;
  virtual std::string tool_name() const OVERRIDE {
    return "MemoryTrackerPrintThread";
  }
};

// Generates CSV values of the engine.
// There are three sections of data including:
//   1. average bytes / alloc
//   2. # Bytes allocated per memory scope.
//   3. # Allocations per memory scope.
// This data can be pasted directly into a Google spreadsheet and visualized.
// Note that this thread will implicitly call Start() is called during
// construction and Cancel() & Join() during destruction.
class MemoryTrackerPrintCSV : public AbstractMemoryTrackerTool {
 public:
  // This tool will only produce on CSV dump of the engine. This is useful
  // for profiling startup memory consumption.
  MemoryTrackerPrintCSV(int sampling_interval_ms, int sampling_time_ms);

  // Overridden so that the thread can exit gracefully.
  virtual void Run(Params* params) OVERRIDE;
  virtual std::string tool_name() const OVERRIDE {
    return "MemoryTrackerPrintCSV";
  }

 private:
  struct AllocationSamples {
    std::vector<int32_t> number_allocations_;
    std::vector<int64_t> allocated_bytes_;
  };
  typedef std::map<std::string, AllocationSamples> MapAllocationSamples;
  static std::string ToCsvString(const MapAllocationSamples& samples);
  static const char* UntrackedMemoryKey();
  bool TimeExpiredYet(const Params& params);

  nb::analytics::MemoryTracker* memory_tracker_;
  const int sample_interval_ms_;
  const int sampling_time_ms_;
};

struct AllocationSamples {
  std::vector<int32_t> number_allocations_;
  std::vector<int64_t> allocated_bytes_;
};
typedef std::map<std::string, AllocationSamples> MapAllocationSamples;
typedef std::vector<base::TimeDelta> TimeStamps;

struct TimeSeries {
  MapAllocationSamples samples_;
  TimeStamps time_stamps_;
};

// Generates CSV values of the engine of a compressed-scale TimeSeries.
//
// A compressed-scale TimeSeries is defined as a time series which has
// higher resolution at the most recent sampling and becomes progressively
// lower resolution as time goes backward.
//
// For example, here is a compressed-histogram of order 2, which loses
// 50% resolution on each section going back.
//
// Startup +------->  +--------+ ..% Resolution
//                    +--------+
//                    |        | 12% Resolution
//                    +--------+
//                    |        |
//                    |        | 25% Resolution
//                    +--------+
//                    |        |
//                    |        |
//                    |        | 50% Resolution
//                    |        |
//                    |        |
//                    +--------+
//                    |        |
//                    |        |
//                    |        |
//                    |        |
//                    |        |
//                    |        |
//                    |        | 100% Resolution
//                    |        |
//                    |        |
//                    |        |
//                    |        |
// End Time +------>  +--------+
//
// The the compressed histogram is dumped out as a CSV string periodically to
// the output stream.
class MemoryTrackerCompressedTimeSeries : public AbstractMemoryTrackerTool {
 public:
  MemoryTrackerCompressedTimeSeries();
  virtual void Run(Params* params) OVERRIDE;
  virtual std::string tool_name() const OVERRIDE {
    return "MemoryTrackerCompressedTimeSeries";
  }

 private:
  static std::string ToCsvString(const TimeSeries& histogram);
  static void AcquireSample(nb::analytics::MemoryTracker* memory_tracker,
                            TimeSeries* histogram, const base::TimeDelta& dt);
  static bool IsFull(const TimeSeries& histogram, size_t num_samples);
  static void Compress(TimeSeries* histogram);

  const int sample_interval_ms_;
  const int number_samples_;
};

// This tool inspects a memory scope and reports on the memory usage.
// The output will be a CSV file printed to stdout representing
// the number of memory allocations for objects. The objects are binned
// according to the size of the memory allocation. Objects within the same
// power of two are binned together. For example 1024 will be binned with 1025.
class MemorySizeBinner : public AbstractMemoryTrackerTool {
 public:
  // memory_scope_name represents the memory scope that is to be investigated.
  explicit MemorySizeBinner(const std::string& memory_scope_name);

  virtual void Run(Params* params) OVERRIDE;
  virtual std::string tool_name() const OVERRIDE {
    return "MemoryTrackerCompressedTimeSeries";
  }

 private:
  std::string memory_scope_name_;
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
  virtual bool Visit(
      const void* memory,
      const nb::analytics::AllocationRecord& alloc_record) OVERRIDE;

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

  virtual bool Visit(
      const void* memory,
      const nb::analytics::AllocationRecord& alloc_record) OVERRIDE;

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

#endif  // COBALT_BROWSER_MEMORY_TRACKER_MEMORY_TRACKER_TOOL_IMPL_H_

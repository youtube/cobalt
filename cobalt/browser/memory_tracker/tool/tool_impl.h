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
  virtual void Flush() = 0;
};

// Params holds important data needed by the MemoryTracker tools during
// their run cycle.
class Params;

//
class AbstractTool {
 public:
  virtual ~AbstractTool() {}
  virtual std::string tool_name() const = 0;
  virtual void Run(Params* params) = 0;
};

class ToolThread : public base::SimpleThread {
 public:
  typedef base::SimpleThread Super;
  ToolThread(nb::analytics::MemoryTracker* memory_tracker, AbstractTool* tool,
             AbstractLogger* logger);
  virtual ~ToolThread();

  virtual void Join() OVERRIDE;
  virtual void Run() OVERRIDE;

 private:
  scoped_ptr<Params> params_;
  scoped_ptr<AbstractTool> tool_;
};

// Start() is called when this object is created, and Cancel() & Join() are
// called during destruction.
class PrintTool : public AbstractTool {
 public:
  PrintTool();
  ~PrintTool() OVERRIDE;

  // Overridden so that the thread can exit gracefully.
  virtual void Run(Params* params) OVERRIDE;
  virtual std::string tool_name() const OVERRIDE {
    return "MemoryTrackerPrintThread";
  }

 private:
  class CvalsMap;
  scoped_ptr<CvalsMap> cvals_map_;
};

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

  const int number_samples_;
};

// This tool inspects a memory scope and reports on the memory usage.
// The output will be a CSV file printed to stdout representing
// the number of memory allocations for objects. The objects are binned
// according to the size of the memory allocation. Objects within the same
// power of two are binned together. For example 1024 will be binned with 1025.
class MemorySizeBinner : public AbstractTool {
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

// Outputs memory_log.txt to the output log location. This log contains
// allocations with a stack trace (non-symbolized) and deallocations without
// a stack.
class LogWriterTool : public AbstractTool {
 public:
  LogWriterTool();
  virtual ~LogWriterTool();

  // Interface AbstrctMemoryTrackerTool
  virtual std::string tool_name() const OVERRIDE;
  virtual void Run(Params* params) OVERRIDE;

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

// Records allocations and outputs leaks as a CSV. Each column will
// have an associated symbol.
class LeakFinderTool : public AbstractTool,
                       public nb::analytics::MemoryTrackerDebugCallback {
 public:
  enum StackTraceMode {
    // Always get the C++ version of the stack trace.
    kCPlusPlus,
    // Whenever possible, get the javascript stack trace,
    // else fallback to CPlusPlus.
    kJavascript,
  };

  explicit LeakFinderTool(StackTraceMode pref);
  virtual ~LeakFinderTool();

  // OnMemoryAllocation() and OnMemoryDeallocation() are part of
  // class MemoryTrackerDebugCallback.
  void OnMemoryAllocation(const void* memory_block,
                          const nb::analytics::AllocationRecord& record,
                          const nb::analytics::CallStack& callstack) OVERRIDE;

  void OnMemoryDeallocation(const void* memory_block,
                            const nb::analytics::AllocationRecord& record,
                            const nb::analytics::CallStack& callstack) OVERRIDE;

  // Interface AbstractMemoryTrackerTool
  virtual std::string tool_name() const OVERRIDE;
  virtual void Run(Params* params) OVERRIDE;

  const std::string* GetOrCreateSymbol(
      const nb::analytics::CallStack& callstack);

  const std::string* TryGetJavascriptSymbol();
  const std::string* GetOrCreateCplusPlusSymbol(
      const nb::analytics::CallStack& callstack);

 private:
  struct AllocRec;
  // A map from callsite -> allocation size.
  // typedef std::map<const std::string*, AllocRec> AllocationFrameMap;
  typedef nb::ConcurrentMap<const std::string*, AllocRec,
                            nb::PODHasher<const std::string*> >
      AllocationFrameMap;

  // A map of memory -> callsite.
  // This keeps track of what callsites belong to which allocations.
  // typedef std::map<const void*, const std::string*> CallFrameMap;
  //
  typedef nb::ConcurrentMap<const void*, const std::string*,
                            nb::PODHasher<const std::string*> > CallFrameMap;

  // A map from callsite -> allocation data.
  typedef std::map<const std::string*, std::vector<AllocRec> > MapSamples;
  // A value type for holding a slope and Y-intercept.
  typedef std::pair<double, double> SlopeYIntercept;
  // An AllocationRecord.
  struct AllocRec {
    AllocRec() : total_bytes(0), num_allocs(0) {}
    AllocRec(const AllocRec& other)
        : total_bytes(other.total_bytes), num_allocs(other.num_allocs) {}
    AllocRec& operator=(const AllocRec& other) {
      total_bytes = other.total_bytes;
      num_allocs = other.num_allocs;
      return *this;
    }
    int64_t total_bytes;
    int32_t num_allocs;
  };

  // This data structure is used to for sorting leaks. The important variable
  // is |leak_potential|, which represents how likely this AllocationProfile
  // is a leak.
  struct AllocationProfile {
    AllocationProfile()
        : name_(NULL),
          alloc_history_(NULL),
          slope_(0),
          y_intercept_(0),
          leak_potential_(0) {}

    AllocationProfile(const std::string* name,
                      const std::vector<AllocRec>* alloc_history, double slope,
                      double y_intercept)
        : name_(name),
          alloc_history_(alloc_history),
          slope_(slope),
          y_intercept_(y_intercept),
          leak_potential_(0) {}
    const std::string* name_;
    const std::vector<AllocRec>* alloc_history_;
    // Linear regression data.
    double slope_;
    double y_intercept_;

    // This this value is set externally. Higher values indicate higher chance
    // that this is a leak.
    double leak_potential_;

    static bool CompareLeakPotential(const AllocationProfile& a,
                                     const AllocationProfile& b) {
      return a.leak_potential_ < b.leak_potential_;
    }
  };

  static std::string GenerateCSV(
      const std::vector<base::TimeDelta>& time_values,
      const std::vector<AllocationProfile>& data);

  void SampleSnapshot(
      std::vector<std::pair<const std::string*, AllocRec> >* destination);

  static void GenerateTopLeakingAllocationProfiles(
      const std::vector<base::TimeDelta>& time_values,
      const MapSamples& samples, std::vector<AllocationProfile>* destination);

  static bool IsJavascriptScope(const nb::analytics::CallStack& callstack);

  const std::string* default_callframe_str_;
  nb::ConcurrentStringInterner string_pool_;
  AllocationFrameMap frame_map_;
  CallFrameMap callframe_map_;
  StackTraceMode stack_trace_mode_;
};
}  // namespace memory_tracker
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_TOOL_IMPL_H_

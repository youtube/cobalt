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

#ifndef COBALT_BROWSER_MEMORY_TRACKER_TOOL_LEAK_FINDER_TOOL_H_
#define COBALT_BROWSER_MEMORY_TRACKER_TOOL_LEAK_FINDER_TOOL_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "cobalt/browser/memory_tracker/tool/tool_impl.h"
#include "nb/analytics/memory_tracker.h"
#include "nb/concurrent_map.h"
#include "nb/hash.h"

namespace cobalt {
namespace browser {
namespace memory_tracker {

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
                          const nb::analytics::CallStack& callstack) override;

  void OnMemoryDeallocation(const void* memory_block,
                            const nb::analytics::AllocationRecord& record,
                            const nb::analytics::CallStack& callstack) override;

  // Interface AbstractMemoryTrackerTool
  std::string tool_name() const override;
  void Run(Params* params) override;

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

#endif  // COBALT_BROWSER_MEMORY_TRACKER_TOOL_LEAK_FINDER_TOOL_H_

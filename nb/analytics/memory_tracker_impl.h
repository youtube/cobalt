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

#ifndef NB_MEMORY_TRACKER_IMPL_H_
#define NB_MEMORY_TRACKER_IMPL_H_

#include "nb/analytics/memory_tracker_helpers.h"
#include "nb/analytics/memory_tracker.h"
#include "nb/memory_scope.h"
#include "nb/scoped_ptr.h"
#include "nb/thread_local_object.h"
#include "starboard/configuration.h"
#include "starboard/memory_reporter.h"
#include "starboard/memory.h"
#include "starboard/mutex.h"
#include "starboard/time.h"

namespace nb {
namespace analytics {

class MemoryTrackerImpl : public MemoryTracker {
 public:
  typedef ConcurrentAllocationMap AllocationMapType;

  MemoryTrackerImpl();
  virtual ~MemoryTrackerImpl();

  // MemoryTracker adapter which is compatible with the SbMemoryReporter
  // interface.
  SbMemoryReporter* GetMemoryReporter();
  NbMemoryScopeReporter* GetMemoryScopeReporter();

  AllocationGroup* GetAllocationGroup(const char* name);
  // Declares the start of a memory region. After this call, all
  // memory regions will be tagged with this allocation group.
  // Note that AllocationGroup is a tracking characteristic and
  // does not imply any sort of special allocation pool.
  void PushAllocationGroupByName(const char* group_name);
  void PushAllocationGroup(AllocationGroup* alloc_group);
  AllocationGroup* PeekAllocationGroup();
  // Ends the current memory region and the previous memory region
  // is restored.
  void PopAllocationGroup();

  // CONTROL
  //
  // Adds tracking to the supplied memory pointer. An AllocationRecord is
  // generated for the supplied allocation which can be queried immediately
  // with GetMemoryTracking(...).
  bool InstallGlobalTrackingHooks() SB_OVERRIDE {
    if (global_hooks_installed_)
      return true;
    global_hooks_installed_ = true;
    bool ok = SbMemorySetReporter(GetMemoryReporter());
    ok |= NbSetMemoryScopeReporter(GetMemoryScopeReporter());
    return ok;
  }
  void RemoveGlobalTrackingHooks() SB_OVERRIDE {
    SbMemorySetReporter(NULL);
    NbSetMemoryScopeReporter(NULL);
  }

  bool AddMemoryTracking(const void* memory, size_t size) SB_OVERRIDE;
  size_t RemoveMemoryTracking(const void* memory) SB_OVERRIDE;
  // Returns true if the allocation record was successfully found.
  // If true then the output will be written to with the values.
  // Otherwise the output is reset to the empty AllocationRecord.
  bool GetMemoryTracking(const void* memory,
                         AllocationRecord* record) const SB_OVERRIDE;
  // Thread local function to get and set the memory tracking state. When set
  // to disabled then memory allocations are not recorded. However memory
  // deletions are still recorded.
  void SetMemoryTrackingEnabled(bool on) SB_OVERRIDE;
  bool IsMemoryTrackingEnabled() const SB_OVERRIDE;

  // REPORTING
  //
  // Total allocation bytes that have been allocated by this
  // MemoryTrackerImpl.
  int64_t GetTotalAllocationBytes() SB_OVERRIDE;
  // Retrieves a collection of all known allocation groups. Locking is done
  // internally.
  void GetAllocationGroups(std::vector<const AllocationGroup*>* output)
      SB_OVERRIDE;
  // Retrieves a collection of all known allocation groups. Locking is done
  // internally. The output is a map of names to AllocationGroups.
  void GetAllocationGroups(
      std::map<std::string, const AllocationGroup*>* output);

  // Provides access to the internal allocations in a thread safe way.
  // Allocation tracking is disabled in the current thread for the duration
  // of the visitation.
  void Accept(AllocationVisitor* visitor) SB_OVERRIDE;

  int64_t GetTotalNumberOfAllocations() SB_OVERRIDE {
    return pointer_map()->Size();
  }

  // TESTING.
  AllocationMapType* pointer_map() { return &atomic_allocation_map_; }
  void Clear();

  // This is useful for debugging. Allows the developer to set a breakpoint
  // and see only allocations that are in the defined allocation group. This
  // is only active in the current thread.
  void Debug_PushAllocationGroupBreakPointByName(const char* group_name);
  void Debug_PushAllocationGroupBreakPoint(AllocationGroup* alloc_group);
  void Debug_PopAllocationGroupBreakPoint();

  // This is useful for testing, setting this to a thread will allow ONLY
  // those allocations from the set thread.
  // Setting this to kSbThreadInvalidId (default) allows all threads to report
  // allocations.
  void SetThreadFilter(SbThreadId tid);
  bool IsCurrentThreadAllowedToReport() const;

 private:
  struct DisableMemoryTrackingInScope {
    DisableMemoryTrackingInScope(MemoryTrackerImpl* t);
    ~DisableMemoryTrackingInScope();
    MemoryTrackerImpl* owner_;
    bool prev_value_;
  };

  // Disables all memory deletion in the current scope. This is used in one
  // location.
  struct DisableDeletionInScope {
    DisableDeletionInScope(MemoryTrackerImpl* owner);
    ~DisableDeletionInScope();
    MemoryTrackerImpl* owner_;
    bool prev_state_;
  };

  // These are functions that are used specifically SbMemoryReporter.
  static void OnMalloc(void* context, const void* memory, size_t size);
  static void OnDealloc(void* context, const void* memory);
  static void OnMapMem(void* context, const void* memory, size_t size);
  static void OnUnMapMem(void* context, const void* memory, size_t size);
  static void OnPushAllocationGroup(void* context,
                                    NbMemoryScopeInfo* memory_scope_info);
  static void OnPopAllocationGroup(void* context);

  void Initialize(SbMemoryReporter* memory_reporter,
                  NbMemoryScopeReporter* nb_memory_scope_reporter);
  void AddAllocationBytes(int64_t val);
  bool MemoryDeletionEnabled() const;

  void SetMemoryDeletionEnabled(bool on);

  SbMemoryReporter sb_memory_tracker_;
  NbMemoryScopeReporter nb_memory_scope_reporter_;
  SbThreadId thread_filter_id_;

  AllocationMapType atomic_allocation_map_;
  AtomicStringAllocationGroupMap alloc_group_map_;

  atomic_int64_t total_bytes_allocated_;

  // THREAD LOCAL SECTION.
  ThreadLocalBoolean memory_deletion_enabled_tls_;
  ThreadLocalBoolean memory_tracking_disabled_tls_;
  ThreadLocalObject<AllocationGroupStack> allocation_group_stack_tls_;
  bool global_hooks_installed_;
};

// Start() is called when this object is created, and Cancel() & Join() are
// called during destruction.
class MemoryTrackerPrintThread : private SimpleThread {
 public:
  MemoryTrackerPrintThread(MemoryTracker* memory_tracker);
  virtual ~MemoryTrackerPrintThread() SB_OVERRIDE;

  // Overridden so that the thread can exit gracefully.
  virtual void Cancel() SB_OVERRIDE;
  virtual void Run() SB_OVERRIDE;

 private:
  atomic_bool finished_;
  MemoryTracker* memory_tracker_;
};

// Generates CSV values of the engine.
// There are three sections of data including:
//   1. average bytes / alloc
//   2. # Bytes allocated per memory scope.
//   3. # Allocations per memory scope.
// This data can be pasted directly into a Google spreadsheet and visualized.
// Note that this thread will implicitly call Start() is called during
// construction and Cancel() & Join() during destruction.
class MemoryTrackerPrintCSVThread : private SimpleThread {
 public:
  MemoryTrackerPrintCSVThread(MemoryTracker* memory_tracker,
                              int sampling_interval_ms,
                              int sampling_time_ms);
  virtual ~MemoryTrackerPrintCSVThread() SB_OVERRIDE;

  // Overridden so that the thread can exit gracefully.
  virtual void Cancel() SB_OVERRIDE;
  virtual void Run() SB_OVERRIDE;

 private:
  struct AllocationSamples {
    std::vector<int32_t> number_allocations_;
    std::vector<int64_t> allocated_bytes_;
  };
  typedef std::map<std::string, AllocationSamples> MapAllocationSamples;
  static std::string ToCsvString(const MapAllocationSamples& samples);
  static const char* UntrackedMemoryKey();
  bool TimeExpiredYet();

  MemoryTracker* memory_tracker_;
  const int sample_interval_ms_;
  const int sampling_time_ms_;
  SbTime start_time_;
  atomic_bool canceled_;
};

}  // namespace analytics
}  // namespace nb

#endif  // NB_MEMORY_TRACKER_IMPL_H_

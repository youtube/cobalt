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

#include "nb/analytics/memory_tracker_impl.h"

#include <iomanip>
#include <sstream>

#include "nb/atomic.h"
#include "starboard/atomic.h"

namespace nb {
namespace analytics {

SbMemoryReporter* MemoryTrackerImpl::GetMemoryReporter() {
  return &sb_memory_tracker_;
}

NbMemoryScopeReporter* MemoryTrackerImpl::GetMemoryScopeReporter() {
  return &nb_memory_scope_reporter_;
}

int64_t MemoryTrackerImpl::GetTotalAllocationBytes() {
  return total_bytes_allocated_.load();
}

AllocationGroup* MemoryTrackerImpl::GetAllocationGroup(const char* name) {
  DisableMemoryTrackingInScope no_tracking(this);
  AllocationGroup* alloc_group = alloc_group_map_.Ensure(name);
  return alloc_group;
}

void MemoryTrackerImpl::PushAllocationGroupByName(const char* group_name) {
  AllocationGroup* group = GetAllocationGroup(group_name);
  PushAllocationGroup(group);
}

void MemoryTrackerImpl::PushAllocationGroup(AllocationGroup* alloc_group) {
  if (alloc_group == NULL) {
    alloc_group = alloc_group_map_.GetDefaultUnaccounted();
  }
  DisableMemoryTrackingInScope no_tracking(this);
  allocation_group_stack_tls_.GetOrCreate()->Push(alloc_group);
}

AllocationGroup* MemoryTrackerImpl::PeekAllocationGroup() {
  DisableMemoryTrackingInScope no_tracking(this);
  AllocationGroup* out =
      allocation_group_stack_tls_.GetOrCreate()->Peek();
  if (out == NULL) {
    out = alloc_group_map_.GetDefaultUnaccounted();
  }
  return out;
}

void MemoryTrackerImpl::PopAllocationGroup() {
  DisableMemoryTrackingInScope no_tracking(this);
  AllocationGroupStack* alloc_tls = allocation_group_stack_tls_.GetOrCreate();
  alloc_tls->Pop();
  AllocationGroup* group = alloc_tls->Peek();
  // We don't allow null, so if this is encountered then push the
  // "default unaccounted" alloc group.
  if (group == NULL) {
    alloc_tls->Push(alloc_group_map_.GetDefaultUnaccounted());
  }
}

void MemoryTrackerImpl::GetAllocationGroups(
    std::vector<const AllocationGroup*>* output) {
  DisableMemoryTrackingInScope no_allocation_tracking(this);
  output->reserve(100);
  alloc_group_map_.GetAll(output);
}

void MemoryTrackerImpl::GetAllocationGroups(
    std::map<std::string, const AllocationGroup*>* output) {
  output->clear();
  DisableMemoryTrackingInScope no_tracking(this);
  std::vector<const AllocationGroup*> tmp;
  GetAllocationGroups(&tmp);
  for (size_t i = 0; i < tmp.size(); ++i) {
    output->insert(std::make_pair(tmp[i]->name(), tmp[i]));
  }
}

void MemoryTrackerImpl::Accept(AllocationVisitor* visitor) {
  DisableMemoryTrackingInScope no_mem_tracking(this);
  atomic_allocation_map_.Accept(visitor);
}

void MemoryTrackerImpl::Clear() {
  // Prevent clearing of the tree from triggering a re-entrant
  // memory deallocation.
  atomic_allocation_map_.Clear();
  total_bytes_allocated_.store(0);
}

void MemoryTrackerImpl::Debug_PushAllocationGroupBreakPointByName(
    const char* group_name) {
  DisableMemoryTrackingInScope no_tracking(this);
  SB_DCHECK(group_name != NULL);
  AllocationGroup* group = alloc_group_map_.Ensure(group_name);
  Debug_PushAllocationGroupBreakPoint(group);
}

void MemoryTrackerImpl::Debug_PushAllocationGroupBreakPoint(
    AllocationGroup* alloc_group) {
  DisableMemoryTrackingInScope no_tracking(this);
  allocation_group_stack_tls_.GetOrCreate()->Push_DebugBreak(alloc_group);
}

void MemoryTrackerImpl::Debug_PopAllocationGroupBreakPoint() {
  DisableMemoryTrackingInScope no_tracking(this);
  allocation_group_stack_tls_.GetOrCreate()->Pop_DebugBreak();
}

// Converts "2345.54" => "2,345.54".
std::string InsertCommasIntoNumberString(const std::string& input) {
  typedef std::vector<char> CharVector;
  typedef CharVector::iterator CharIt;

  CharVector chars(input.begin(), input.end());
  std::reverse(chars.begin(), chars.end());

  CharIt curr_it = chars.begin();
  CharIt mid = std::find(chars.begin(), chars.end(), '.');
  if (mid == chars.end()) {
    mid = curr_it;
  }

  CharVector out(curr_it, mid);

  int counter = 0;
  for (CharIt it = mid; it != chars.end(); ++it) {
    if (counter != 0 && (counter % 3 == 0)) {
      out.push_back(',');
    }
    if (*it != '.') {
      counter++;
    }
    out.push_back(*it);
  }

  std::reverse(out.begin(), out.end());
  std::stringstream ss;
  for (int i = 0; i < out.size(); ++i) {
    ss << out[i];
  }
  return ss.str();
}

template <typename T>
std::string NumberFormatWithCommas(T val) {
  // Convert value to string.
  std::stringstream ss;
  ss << val;
  std::string s = InsertCommasIntoNumberString(ss.str());
  return s;
}

void MemoryTrackerImpl::OnMalloc(void* context,
                                 const void* memory,
                                 size_t size) {
  MemoryTrackerImpl* t = static_cast<MemoryTrackerImpl*>(context);
  t->AddMemoryTracking(memory, size);
}

void MemoryTrackerImpl::OnDealloc(void* context, const void* memory) {
  MemoryTrackerImpl* t = static_cast<MemoryTrackerImpl*>(context);
  t->RemoveMemoryTracking(memory);
}

void MemoryTrackerImpl::OnMapMem(void* context,
                                 const void* memory,
                                 size_t size) {
  // We might do something more interesting with MapMemory calls later.
  OnMalloc(context, memory, size);
}

void MemoryTrackerImpl::OnUnMapMem(void* context,
                                   const void* memory,
                                   size_t size) {
  // We might do something more interesting with UnMapMemory calls later.
  OnDealloc(context, memory);
}

void MemoryTrackerImpl::OnPushAllocationGroup(
    void* context,
    NbMemoryScopeInfo* memory_scope_info) {
  MemoryTrackerImpl* t = static_cast<MemoryTrackerImpl*>(context);
  uintptr_t* cached_handle = &(memory_scope_info->cached_handle_);
  const bool allows_caching = memory_scope_info->allows_caching_;
  const char* group_name = memory_scope_info->memory_scope_name_;

  AllocationGroup* group = NULL;
  if (allows_caching && *cached_handle != 0) {
    group = reinterpret_cast<AllocationGroup*>(cached_handle);
  } else {
    group = t->GetAllocationGroup(group_name);
    if (allows_caching) {
      // Flush all pending writes so that the the pointee is well formed
      // by the time the pointer becomes visible to other threads.
      SbAtomicMemoryBarrier();
      *cached_handle = reinterpret_cast<uintptr_t>(group);
    }
  }

  t->PushAllocationGroup(group);
}

void MemoryTrackerImpl::OnPopAllocationGroup(void* context) {
  MemoryTrackerImpl* t = static_cast<MemoryTrackerImpl*>(context);
  t->PopAllocationGroup();
}

void MemoryTrackerImpl::Initialize(
    SbMemoryReporter* sb_memory_reporter,
    NbMemoryScopeReporter* memory_scope_reporter) {
  SbMemoryReporter mem_reporter = {
      MemoryTrackerImpl::OnMalloc, MemoryTrackerImpl::OnDealloc,

      MemoryTrackerImpl::OnMapMem, MemoryTrackerImpl::OnUnMapMem,

      this};

  NbMemoryScopeReporter mem_scope_reporter = {
      MemoryTrackerImpl::OnPushAllocationGroup,
      MemoryTrackerImpl::OnPopAllocationGroup,

      this,
  };

  *sb_memory_reporter = mem_reporter;
  *memory_scope_reporter = mem_scope_reporter;
}

void MemoryTrackerImpl::SetThreadFilter(SbThreadId tid) {
  thread_filter_id_ = tid;
}

bool MemoryTrackerImpl::IsCurrentThreadAllowedToReport() const {
  if (thread_filter_id_ == kSbThreadInvalidId) {
    return true;
  }
  return SbThreadGetId() == thread_filter_id_;
}

MemoryTrackerImpl::DisableDeletionInScope::DisableDeletionInScope(
    MemoryTrackerImpl* owner)
    : owner_(owner) {
  prev_state_ = owner->MemoryDeletionEnabled();
  owner_->SetMemoryDeletionEnabled(false);
}

MemoryTrackerImpl::DisableDeletionInScope::~DisableDeletionInScope() {
  owner_->SetMemoryDeletionEnabled(prev_state_);
}

class MemoryTrackerPrintThread : public SimpleThread {
 public:
  MemoryTrackerPrintThread(MemoryTracker* owner)
      : SimpleThread("MemoryTrackerPrintThread"),
        finished_(false),
        owner_(owner) {}

  // Overridden so that the thread can exit gracefully.
  virtual void Cancel() SB_OVERRIDE { finished_.store(true); }

  virtual void Run() {
    struct NoMemTracking {
      NoMemTracking(MemoryTracker* owner) : owner_(owner) {
        prev_val_ = owner_->IsMemoryTrackingEnabled();
        owner_->SetMemoryTrackingEnabled(false);
      }
      ~NoMemTracking() { owner_->SetMemoryTrackingEnabled(prev_val_); }

      bool prev_val_;
      MemoryTracker* owner_;
    };

    while (!finished_.load()) {
      NoMemTracking no_mem_tracking_in_this_scope(owner_);

      // std::map<std::string, const AllocationGroup*> output;
      // typedef std::map<std::string, const AllocationGroup*>::const_iterator
      // MapIt;
      std::vector<const AllocationGroup*> vector_output;
      owner_->GetAllocationGroups(&vector_output);

      typedef std::map<std::string, const AllocationGroup*> Map;
      typedef Map::const_iterator MapIt;

      Map output;
      for (int i = 0; i < vector_output.size(); ++i) {
        const AllocationGroup* group = vector_output[i];
        output[group->name()] = group;
      }

      int32_t num_allocs = 0;
      int64_t total_bytes = 0;

      struct F {
        static void PrintRow(std::stringstream& ss,
                             const std::string& v1,
                             const std::string& v2,
                             const std::string& v3) {
          ss.width(20);
          ss << std::left << v1;
          ss.width(13);
          ss << std::right << v2 << "  ";
          ss.width(7);
          ss << std::right << v3 << "\n";
        }
      };

      if (owner_->IsMemoryTrackingEnabled()) {
        // If this isn't true then it would cause an infinite loop. The
        // following will likely crash.
        SB_DCHECK(false) << "Unexpected, memory tracking should be disabled.";
      }

      std::stringstream ss;
      for (MapIt it = output.begin(); it != output.end(); ++it) {
        const AllocationGroup* group = it->second;
        if (!group) {
          continue;
        }

        int32_t num_group_allocs = -1;
        int64_t total_group_bytes = -1;

        group->GetAggregateStats(&num_group_allocs, &total_group_bytes);
        SB_DCHECK(-1 != num_group_allocs);
        SB_DCHECK(-1 != total_group_bytes);
        num_allocs += num_group_allocs;
        total_bytes += total_group_bytes;

        ss.width(20);
        ss << std::left << it->first;
        ss.width(13);
        ss << std::right << NumberFormatWithCommas(total_group_bytes) << "  ";
        ss.width(7);
        ss << std::right << NumberFormatWithCommas(num_group_allocs) << "\n";
      }
      ss << "-------------------------------\n";

      SB_LOG(INFO) << "\n"
                   << "Total Bytes Allocated: "
                   << NumberFormatWithCommas(total_bytes) << "\n"
                   << "Total allocations: "
                   << NumberFormatWithCommas(num_allocs) << "\n\n" << ss.str();

      SbThreadSleep(250);
    }
  }

 private:
  atomic_bool finished_;
  MemoryTracker* owner_;
};

void MemoryTrackerImpl::Debug_EnablePrintOutThread() {
  if (debug_output_thread_) {
    return;
  }  // Already enabled.
  debug_output_thread_.reset(new MemoryTrackerPrintThread(this));
  debug_output_thread_->Start();
}

MemoryTrackerImpl::MemoryTrackerImpl()
    : thread_filter_id_(kSbThreadInvalidId) {
  total_bytes_allocated_.store(0);
  global_hooks_installed_ = false;
  Initialize(&sb_memory_tracker_, &nb_memory_scope_reporter_);
  // Push the default region so that stats can be accounted for.
  PushAllocationGroup(alloc_group_map_.GetDefaultUnaccounted());
}

MemoryTrackerImpl::~MemoryTrackerImpl() {
  // If we are currently hooked into allocation tracking...
  if (global_hooks_installed_) {
    SbMemorySetReporter(NULL);
    // For performance reasons no locking is used on the tracker.
    // Therefore give enough time for other threads to exit this tracker
    // before fully destroying this object.
    SbThreadSleep(250 * kSbTimeMillisecond);  // 250 millisecond wait.
  }
  if (debug_output_thread_) {
    debug_output_thread_->Cancel();
    debug_output_thread_->Join();
    debug_output_thread_.reset();
  }
}

bool MemoryTrackerImpl::AddMemoryTracking(const void* memory, size_t size) {
  // Vars are stored to assist in debugging.
  const bool thread_allowed_to_report = IsCurrentThreadAllowedToReport();
  const bool valid_memory_request = (memory != NULL) && (size != 0);
  const bool mem_track_enabled = IsMemoryTrackingEnabled();

  const bool tracking_enabled =
      mem_track_enabled && valid_memory_request && thread_allowed_to_report;

  if (!tracking_enabled) {
    return false;
  }

  // End all memory tracking in subsequent data structures.
  DisableMemoryTrackingInScope no_memory_tracking(this);
  AllocationGroupStack* alloc_stack =
      allocation_group_stack_tls_.GetOrCreate();
  AllocationGroup* group = alloc_stack->Peek();
  if (!group) {
    group = alloc_group_map_.GetDefaultUnaccounted();
  }

#ifndef NDEBUG
  // This section of the code is designed to allow a developer to break
  // execution whenever the debug allocation stack is in scope, so that the
  // allocations can be stepped through.
  // Example:
  //   Debug_PushAllocationGroupBreakPointByName("Javascript");
  //  ...now set a break point below at "static int i = 0"
  if (group && (group == alloc_stack->Peek_DebugBreak()) &&
      alloc_stack->Peek_DebugBreak()) {
    static int i = 0;  // This static is here to allow an
    ++i;               // easy breakpoint in the debugger
  }
#endif

  AllocationRecord alloc_record(size, group);
  bool added = atomic_allocation_map_.Add(memory, alloc_record);
  if (added) {
    AddAllocationBytes(size);
    group->AddAllocation(size);
  } else {
    AllocationRecord unexpected_alloc;
    atomic_allocation_map_.Get(memory, &unexpected_alloc);
    AllocationGroup* prev_group = unexpected_alloc.allocation_group;

    std::string prev_group_name;
    if (prev_group) {
      prev_group_name = unexpected_alloc.allocation_group->name();
    } else {
      prev_group_name = "none";
    }

    SB_DCHECK(added)
        << "\nUnexpected condition, previous allocation was not removed:\n"
        << "\tprevious alloc group: " << prev_group_name << "\n"
        << "\tnew alloc group: " << group->name() << "\n"
        << "\tprevious size: " << unexpected_alloc.size << "\n"
        << "\tnew size: " << size << "\n";
  }
  return added;
}

size_t MemoryTrackerImpl::RemoveMemoryTracking(const void* memory) {
  const bool do_remove = memory && MemoryDeletionEnabled();
  if (!do_remove) {
    return 0;
  }

  AllocationRecord alloc_record;
  bool removed = false;

  // Prevent a map::erase() from causing an endless stack overflow by
  // disabling memory deletion for the very limited scope.
  {
    // Not a correct name. TODO - change.
    DisableDeletionInScope no_memory_deletion(this);
    removed = atomic_allocation_map_.Remove(memory, &alloc_record);
  }

  if (!removed) {
    return 0;
  } else {
    const int64_t alloc_size = (static_cast<int64_t>(alloc_record.size));
    AllocationGroup* group = alloc_record.allocation_group;
    if (group) {
      group->AddAllocation(-alloc_size);
    }
    AddAllocationBytes(-alloc_size);
    return alloc_record.size;
  }
}

bool MemoryTrackerImpl::GetMemoryTracking(const void* memory,
                                          AllocationRecord* record) const {
  const bool exists = atomic_allocation_map_.Get(memory, record);
  return exists;
}

void MemoryTrackerImpl::SetMemoryTrackingEnabled(bool on) {
  memory_tracking_disabled_tls_.Set(!on);
}

bool MemoryTrackerImpl::IsMemoryTrackingEnabled() const {
  const bool enabled = !memory_tracking_disabled_tls_.Get();
  return enabled;
}

void MemoryTrackerImpl::AddAllocationBytes(int64_t val) {
  total_bytes_allocated_.fetch_add(val);
}

bool MemoryTrackerImpl::MemoryDeletionEnabled() const {
  return !memory_deletion_enabled_tls_.Get();
}

void MemoryTrackerImpl::SetMemoryDeletionEnabled(bool on) {
  memory_deletion_enabled_tls_.Set(!on);
}

MemoryTrackerImpl::DisableMemoryTrackingInScope::DisableMemoryTrackingInScope(
    MemoryTrackerImpl* t)
    : owner_(t) {
  prev_value_ = owner_->IsMemoryTrackingEnabled();
  owner_->SetMemoryTrackingEnabled(false);
}

MemoryTrackerImpl::DisableMemoryTrackingInScope::
    ~DisableMemoryTrackingInScope() {
  owner_->SetMemoryTrackingEnabled(prev_value_);
}

}  // namespace analytics
}  // namespace nb

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

#include <cstring>
#include <iomanip>
#include <iterator>
#include <sstream>

#include "starboard/atomic.h"
#include "starboard/log.h"
#include "starboard/time.h"

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
  DisableMemoryTrackingInScope no_mem_allocation_reporting(this);
  DisableDeletionInScope no_mem_deletion_reporting(this);
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
  void** cached_handle = &(memory_scope_info->cached_handle_);
  const bool allows_caching = memory_scope_info->allows_caching_;
  const char* group_name = memory_scope_info->memory_scope_name_;

  AllocationGroup* group = NULL;
  if (allows_caching && *cached_handle != NULL) {
    group = static_cast<AllocationGroup*>(*cached_handle);
  } else {
    group = t->GetAllocationGroup(group_name);
    if (allows_caching) {
      // Flush all pending writes so that the the pointee is well formed
      // by the time the pointer becomes visible to other threads.
      SbAtomicMemoryBarrier();
      *cached_handle = static_cast<void*>(group);
    }
  }

  DisableMemoryTrackingInScope no_memory_tracking_in_scope(t);
  t->PushAllocationGroup(group);
  CallStack* callstack = t->callstack_tls_.GetOrCreate();
  callstack->push_back(memory_scope_info);
}

void MemoryTrackerImpl::OnPopAllocationGroup(void* context) {
  MemoryTrackerImpl* t = static_cast<MemoryTrackerImpl*>(context);
  t->PopAllocationGroup();
  CallStack* callstack = t->callstack_tls_.GetOrCreate();
  // Callstack can be empty when the memory tracker binds to the callback
  // system while the program is in the middle of the execution.
  if (!callstack->empty()) {
    callstack->pop_back();
  }
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

void MemoryTrackerImpl::SetMemoryTrackerDebugCallback(
    MemoryTrackerDebugCallback* cb) {
  debug_callback_.swap(cb);
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

MemoryTrackerImpl::MemoryTrackerImpl()
    : thread_filter_id_(kSbThreadInvalidId), debug_callback_(nullptr) {
  total_bytes_allocated_.store(0);
  global_hooks_installed_ = false;
  Initialize(&sb_memory_tracker_, &nb_memory_scope_reporter_);
  // Push the default region so that stats can be accounted for.
  PushAllocationGroup(alloc_group_map_.GetDefaultUnaccounted());
}

MemoryTrackerImpl::~MemoryTrackerImpl() {
  // If we are currently hooked into allocation tracking...
  if (global_hooks_installed_) {
    RemoveGlobalTrackingHooks();
    // For performance reasons no locking is used on the tracker.
    // Therefore give enough time for other threads to exit this tracker
    // before fully destroying this object.
    SbThreadSleep(250 * kSbTimeMillisecond);  // 250 millisecond wait.
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
  // allocations can be stepped through. This is a THREAD LOCAL operation.
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
    ConcurrentPtr<MemoryTrackerDebugCallback>::Access access_ptr =
        debug_callback_.access_ptr(SbThreadId());

    // If access_ptr is valid then it is guaranteed to be alive for the
    // duration of the scope.
    if (access_ptr) {
      const CallStack& callstack = *(callstack_tls_.GetOrCreate());
      DisableDeletionInScope disable_deletion_tracking(this);
      access_ptr->OnMemoryAllocation(memory, alloc_record, callstack);
    }
  } else {
    // Handles the case where the memory hasn't been properly been reported
    // released. This is less serious than you would think because the memory
    // allocator in the system will recycle the memory and it will come back.
    AllocationRecord unexpected_alloc;
    atomic_allocation_map_.Get(memory, &unexpected_alloc);
    AllocationGroup* prev_group = unexpected_alloc.allocation_group;

    std::string prev_group_name;
    if (prev_group) {
      prev_group_name = unexpected_alloc.allocation_group->name();
    } else {
      prev_group_name = "none";
    }

    if (!added) {
      std::stringstream ss;
      ss << "\nUnexpected condition, previous allocation was not removed:\n"
         << "\tprevious alloc group: " << prev_group_name << "\n"
         << "\tnew alloc group: " << group->name() << "\n"
         << "\tprevious size: " << unexpected_alloc.size << "\n"
         << "\tnew size: " << size << "\n";

      SbLogRaw(ss.str().c_str());
    }
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

  ConcurrentPtr<MemoryTrackerDebugCallback>::Access access_ptr =
      debug_callback_.access_ptr(SbThreadId());

  if (access_ptr) {
    if (atomic_allocation_map_.Get(memory, &alloc_record)) {
      DisableMemoryTrackingInScope no_memory_tracking(this);
      const CallStack& callstack = (*callstack_tls_.GetOrCreate());
      access_ptr->OnMemoryDeallocation(memory, alloc_record, callstack);
    }
  }

  // Prevent a map::erase() from causing an endless stack overflow by
  // disabling memory deletion for the very limited scope.
  {
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

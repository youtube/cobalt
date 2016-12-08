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

#include "nb/atomic.h"
#include "starboard/atomic.h"
#include "starboard/log.h"
#include "starboard/time.h"

namespace nb {
namespace analytics {
namespace {
// NoMemoryTracking will disable memory tracking while in the current scope of
// execution. When the object is destroyed it will reset the previous state
// of allocation tracking.
// Example:
//   void Foo() {
//     NoMemoryTracking no_memory_tracking_in_scope;
//     int* ptr = new int();  // ptr is not tracked.
//     delete ptr;
//     return;    // Previous memory tracking state is restored.
//   }
class NoMemoryTracking {
 public:
  NoMemoryTracking(MemoryTracker* owner) : owner_(owner) {
    prev_val_ = owner_->IsMemoryTrackingEnabled();
    owner_->SetMemoryTrackingEnabled(false);
  }
  ~NoMemoryTracking() { owner_->SetMemoryTrackingEnabled(prev_val_); }

 private:
  bool prev_val_;
  MemoryTracker* owner_;
};

// This is a simple algorithm to remove the "needle" from the haystack. Note
// that this function is simple and not well optimized.
std::string RemoveString(const std::string& haystack, const char* needle) {
  const size_t NOT_FOUND = std::string::npos;

  // Base case. No modification needed.
  size_t pos = haystack.find(needle);
  if (pos == NOT_FOUND) {
    return haystack;
  }
  const size_t n = strlen(needle);
  std::string output;
  output.reserve(haystack.size());

  // Copy string, omitting the portion containing the "needle".
  std::copy(haystack.begin(), haystack.begin() + pos,
            std::back_inserter(output));
  std::copy(haystack.begin() + pos + n, haystack.end(),
            std::back_inserter(output));

  // Recursively remove same needle in haystack.
  return RemoveString(output, needle);
}

}  // namespace

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


MemoryTrackerPrintThread::MemoryTrackerPrintThread(
    MemoryTracker* memory_tracker)
    : SimpleThread("MemoryTrackerPrintThread"),
    finished_(false),
    memory_tracker_(memory_tracker) {
  Start();
}

MemoryTrackerPrintThread::~MemoryTrackerPrintThread() {
  Cancel();
  Join();
}

void MemoryTrackerPrintThread::Cancel() {
  finished_.store(true);
}

void MemoryTrackerPrintThread::Run() {
  while (!finished_.load()) {
    NoMemoryTracking no_mem_tracking_in_this_scope(memory_tracker_);

    std::vector<const AllocationGroup*> vector_output;
    memory_tracker_->GetAllocationGroups(&vector_output);

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
      static void PrintRow(std::stringstream* ss,
          const std::string& v1,
          const std::string& v2,
          const std::string& v3) {
        ss->width(20);
        *ss << std::left << v1;
        ss->width(13);
        *ss << std::right << v2 << "  ";
        ss->width(10);
        *ss << std::right << v3 << "\n";
      }
    };

    if (memory_tracker_->IsMemoryTrackingEnabled()) {
      // If this isn't true then it would cause an infinite loop. The
      // following will likely crash.
      SB_DCHECK(false) << "Unexpected, memory tracking should be disabled.";
    }

    std::stringstream ss;

    F::PrintRow(&ss, "NAME", "BYTES", "NUM ALLOCS");
    ss << "---------------------------------------------\n";
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

      F::PrintRow(&ss,
                  it->first,
                  NumberFormatWithCommas(total_group_bytes),
                  NumberFormatWithCommas(num_group_allocs));
    }
    ss << "---------------------------------------------\n";

    std::stringstream final_ss;
    final_ss << "\n"
      << "Total Bytes Allocated: "
      << NumberFormatWithCommas(total_bytes) << "\n"
      << "Total allocations: "
      << NumberFormatWithCommas(num_allocs) << "\n\n" << ss.str();

    SbLogRaw(final_ss.str().c_str());

    SbThreadSleep(1000 * 1000);
  }
}

MemoryTrackerPrintCSVThread::MemoryTrackerPrintCSVThread(
    MemoryTracker* memory_tracker,
    int sampling_interval_ms,
    int sampling_time_ms)
    : SimpleThread("MemoryTrackerPrintCSVThread"),
      memory_tracker_(memory_tracker),
      sample_interval_ms_(sampling_interval_ms),
      sampling_time_ms_(sampling_time_ms),
      start_time_(SbTimeGetNow()) {
  Start();
}


MemoryTrackerPrintCSVThread::~MemoryTrackerPrintCSVThread() {
  Cancel();
  Join();
}

void MemoryTrackerPrintCSVThread::Cancel() {
  canceled_.store(true);
}

std::string MemoryTrackerPrintCSVThread::ToCsvString(
    const MapAllocationSamples& samples_in) {
  typedef MapAllocationSamples Map;
  typedef Map::const_iterator MapIt;

  const char QUOTE[] = "\"";
  const char DELIM[] = ",";
  const char NEW_LINE[] = "\n";

  size_t largest_sample_size = 0;
  size_t smallest_sample_size = INT_MAX;

  // Sanitize samples_in and store as samples.
  MapAllocationSamples samples;
  for (MapIt it = samples_in.begin(); it != samples_in.end(); ++it) {
    std::string name = it->first;
    const AllocationSamples& value = it->second;

    if (value.allocated_bytes_.size() != value.number_allocations_.size()) {
      SB_NOTREACHED() << "Error at " << __FILE__ << ":" << __LINE__;
      return "ERROR";
    }

    const size_t n = value.allocated_bytes_.size();
    if (n > largest_sample_size) {
      largest_sample_size = n;
    }
    if (n < smallest_sample_size) {
      smallest_sample_size = n;
    }

    // Strip out any characters that could make parsing the csv difficult.
    name = RemoveString(name, QUOTE);
    name = RemoveString(name, DELIM);
    name = RemoveString(name, NEW_LINE);

    const bool duplicate_found = (samples.end() != samples.find(name));
    if (duplicate_found) {
      SB_NOTREACHED() << "Error, duplicate found for entry: "
                      << name << NEW_LINE;
    }
    // Store value as a sanitized sample.
    samples[name] = value;
  }

  SB_DCHECK(largest_sample_size == smallest_sample_size);

  std::stringstream ss;

  // Begin output to CSV.
  // Sometimes we need to skip the CPU memory entry.
  const MapIt total_cpu_memory_it = samples.find(UntrackedMemoryKey());

  // Preamble
  ss << NEW_LINE << "//////////////////////////////////////////////";
  ss << NEW_LINE << "// CSV of bytes / allocation" << NEW_LINE;
  // HEADER.
  ss << "Name" << DELIM << QUOTE << "Bytes/Alloc" << QUOTE << NEW_LINE;
  // DATA.
  for (MapIt it = samples.begin(); it != samples.end(); ++it) {
    if (total_cpu_memory_it == it) {
      continue;
    }

    const AllocationSamples& samples = it->second;
    if (samples.allocated_bytes_.empty() ||
        samples.number_allocations_.empty()) {
      SB_NOTREACHED() << "Should not be here";
      return "ERROR";
    }
    const int32_t n_allocs = samples.number_allocations_.back();
    const int64_t n_bytes = samples.allocated_bytes_.back();
    int bytes_per_alloc = 0;
    if (n_allocs > 0) {
      bytes_per_alloc = n_bytes / n_allocs;
    }
    const std::string& name = it->first;
    ss << QUOTE << name << QUOTE << DELIM << bytes_per_alloc << NEW_LINE;
  }
  ss << NEW_LINE;

  // Preamble
  ss << NEW_LINE << "//////////////////////////////////////////////"
     << NEW_LINE << "// CSV of bytes allocated per region (MB's)."
     << NEW_LINE << "// Units are in Megabytes. This is designed"
     << NEW_LINE << "// to be used in a stacked graph." << NEW_LINE;

  // HEADER.
  for (MapIt it = samples.begin(); it != samples.end(); ++it) {
    if (total_cpu_memory_it == it) {
      continue;
    }
    const std::string& name = it->first;
    ss << QUOTE << name << QUOTE << DELIM;
  }
  // Save the total for last.
  if (total_cpu_memory_it != samples.end()) {
    const std::string& name = total_cpu_memory_it->first;
    ss << QUOTE << name << QUOTE << DELIM;
  }
  ss << NEW_LINE;

  // Print out the values of each of the samples.
  for (int i = 0; i < smallest_sample_size; ++i) {
    for (MapIt it = samples.begin(); it != samples.end(); ++it) {
      if (total_cpu_memory_it == it) {
        continue;
      }
      const int64_t alloc_bytes = it->second.allocated_bytes_[i];
      // Convert to float megabytes with decimals of precision.
      double n = alloc_bytes / (1000 * 10);
      n = n / (100.);
      ss << n << DELIM;
    }
    if (total_cpu_memory_it != samples.end()) {
      const int64_t alloc_bytes =
         total_cpu_memory_it->second.allocated_bytes_[i];
      // Convert to float megabytes with decimals of precision.
      double n = alloc_bytes / (1000 * 10);
      n = n / (100.);
      ss << n << DELIM;
    }
    ss << NEW_LINE;
  }

  ss << NEW_LINE;
  // Preamble
  ss << NEW_LINE << "//////////////////////////////////////////////";
  ss << NEW_LINE << "// CSV of number of allocations per region." << NEW_LINE;

  // HEADER
  for (MapIt it = samples.begin(); it != samples.end(); ++it) {
    if (total_cpu_memory_it == it) {
      continue;
    }
    const std::string& name = it->first;
    ss << QUOTE << name << QUOTE << DELIM;
  }
  ss << NEW_LINE;
  for (int i = 0; i < smallest_sample_size; ++i) {
    for (MapIt it = samples.begin(); it != samples.end(); ++it) {
      if (total_cpu_memory_it == it) {
        continue;
      }
      const int64_t n_allocs = it->second.number_allocations_[i];
      ss << n_allocs << DELIM;
    }
    ss << NEW_LINE;
  }
  std::string output = ss.str();
  return output;
}

const char* MemoryTrackerPrintCSVThread::UntrackedMemoryKey() {
  return "Untracked Memory";
}

void MemoryTrackerPrintCSVThread::Run() {
  NoMemoryTracking no_mem_tracking_in_this_scope(memory_tracker_);

  SbLogRaw("\nMemoryTrackerPrintCSVThread is sampling...\n");
  int sample_count = 0;
  MapAllocationSamples map_samples;

  while (!TimeExpiredYet() && !canceled_.load()) {
    // Sample total memory used by the system.
    MemoryStats mem_stats = GetProcessMemoryStats();
    int64_t untracked_used_memory = mem_stats.used_cpu_memory +
                                    mem_stats.used_gpu_memory;

    std::vector<const AllocationGroup*> vector_output;
    memory_tracker_->GetAllocationGroups(&vector_output);

    // Sample all known memory scopes.
    for (int i = 0; i < vector_output.size(); ++i) {
      const AllocationGroup* group = vector_output[i];
      const std::string& name = group->name();

      const bool first_creation = map_samples.find(group->name()) ==
                                  map_samples.end();

      AllocationSamples* new_entry  = &(map_samples[name]);

      // Didn't see it before so create new entry.
      if (first_creation) {
        // Make up for lost samples...
        new_entry->allocated_bytes_.resize(sample_count, 0);
        new_entry->number_allocations_.resize(sample_count, 0);
      }

      int32_t num_allocs = - 1;
      int64_t allocation_bytes = -1;
      group->GetAggregateStats(&num_allocs, &allocation_bytes);

      new_entry->allocated_bytes_.push_back(allocation_bytes);
      new_entry->number_allocations_.push_back(num_allocs);

      untracked_used_memory -= allocation_bytes;
    }

    // Now push in remaining total.
    AllocationSamples* process_sample = &(map_samples[UntrackedMemoryKey()]);
    if (untracked_used_memory < 0) {
      // On some platforms, total GPU memory may not be correctly reported.
      // However the allocations from the GPU memory may be reported. In this
      // case untracked_used_memory will go negative. To protect the memory
      // reporting the untracked_used_memory is set to 0 so that it doesn't
      // cause an error in reporting.
      untracked_used_memory = 0;
    }
    process_sample->allocated_bytes_.push_back(untracked_used_memory);
    process_sample->number_allocations_.push_back(-1);

    ++sample_count;
    SbThreadSleep(kSbTimeMillisecond * sample_interval_ms_);
  }

  std::string output = ToCsvString(map_samples);
  SbLogRaw(output.c_str());
  SbLogFlush();
  // Prevents the "thread exited code 0" from being interleaved into the
  // output. This happens if SbLogFlush() is not implemented for the platform.
  SbThreadSleep(1000 * kSbTimeMillisecond);
}

bool MemoryTrackerPrintCSVThread::TimeExpiredYet() {
  const SbTime diff_us = SbTimeGetNow() - start_time_;
  const bool expired_time = diff_us > (sampling_time_ms_ * kSbTimeMillisecond);
  return expired_time;
}

///////////////////////////////////////////////////////////////////////////////
MemoryTrackerCompressedTimeSeriesThread::MemoryTrackerCompressedTimeSeriesThread(
    MemoryTracker* memory_tracker)
    : SimpleThread("CompressedScaleHistogram"),
      sample_interval_ms_(100),
      number_samples_(400),
      memory_tracker_(memory_tracker) {
  Start();
}


MemoryTrackerCompressedTimeSeriesThread::~MemoryTrackerCompressedTimeSeriesThread() {
  Cancel();
  Join();
}

void MemoryTrackerCompressedTimeSeriesThread::Cancel() {
  canceled_.store(true);
}

void MemoryTrackerCompressedTimeSeriesThread::Run() {
  TimeSeries timeseries;
  SbTime start_time = SbTimeGetNow();
  while (!canceled_.load()) {
    SbTime now_time = SbTimeGetNow() - start_time;
    AcquireSample(memory_tracker_, &timeseries, now_time);
    if (IsFull(timeseries, number_samples_)) {
      const std::string str = ToCsvString(timeseries);
      Compress(&timeseries);
      Print(str);
    }
    SbThreadSleep(kSbTimeMillisecond * sample_interval_ms_);
  }
}

std::string MemoryTrackerCompressedTimeSeriesThread::ToCsvString(
    const TimeSeries& timeseries) {

  const char QUOTE[] = "\"";
  const char DELIM[] = ",";
  const char NEW_LINE[] = "\n";

  size_t largest_sample_size = 0;
  size_t smallest_sample_size = INT_MAX;


  typedef MapAllocationSamples::const_iterator MapIt;

  // Sanitize samples_in and store as samples.
  const MapAllocationSamples& samples_in = timeseries.samples_;
  MapAllocationSamples samples;
  for (MapIt it = samples_in.begin(); it != samples_in.end(); ++it) {
    std::string name = it->first;
    const AllocationSamples& value = it->second;

    if (value.allocated_bytes_.size() != value.number_allocations_.size()) {
      SB_NOTREACHED() << "Error at " << __FILE__ << ":" << __LINE__;
      return "ERROR";
    }

    const size_t n = value.allocated_bytes_.size();
    if (n > largest_sample_size) {
      largest_sample_size = n;
    }
    if (n < smallest_sample_size) {
      smallest_sample_size = n;
    }

    // Strip out any characters that could make parsing the csv difficult.
    name = RemoveString(name, QUOTE);
    name = RemoveString(name, DELIM);
    name = RemoveString(name, NEW_LINE);

    const bool duplicate_found = (samples.end() != samples.find(name));
    if (duplicate_found) {
      SB_NOTREACHED() << "Error, duplicate found for entry: "
                      << name << NEW_LINE;
    }
    // Store value as a sanitized sample.
    samples[name] = value;
  }

  SB_DCHECK(largest_sample_size == smallest_sample_size);

  std::stringstream ss;

  // Begin output to CSV.

  // Preamble
  ss << NEW_LINE << "//////////////////////////////////////////////"
     << NEW_LINE << "// CSV of bytes allocated per region (MB's)."
     << NEW_LINE << "// Units are in Megabytes. This is designed"
     << NEW_LINE << "// to be used in a stacked graph." << NEW_LINE;

  // HEADER.
  for (MapIt it = samples.begin(); it != samples.end(); ++it) {
    const std::string& name = it->first;
    ss << QUOTE << name << QUOTE << DELIM;
  }
  ss << NEW_LINE;

  // Print out the values of each of the samples.
  for (int i = 0; i < smallest_sample_size; ++i) {
    for (MapIt it = samples.begin(); it != samples.end(); ++it) {
      const int64_t alloc_bytes = it->second.allocated_bytes_[i];
      // Convert to float megabytes with decimals of precision.
      double n = alloc_bytes / (1000 * 10);
      n = n / (100.);
      ss << n << DELIM;
    }
    ss << NEW_LINE;
  }

  ss << NEW_LINE;
  // Preamble
  ss << NEW_LINE << "//////////////////////////////////////////////";
  ss << NEW_LINE << "// CSV of number of allocations per region." << NEW_LINE;

  // HEADER
  for (MapIt it = samples.begin(); it != samples.end(); ++it) {
    const std::string& name = it->first;
    ss << QUOTE << name << QUOTE << DELIM;
  }
  ss << NEW_LINE;
  for (int i = 0; i < smallest_sample_size; ++i) {
    for (MapIt it = samples.begin(); it != samples.end(); ++it) {
      const int64_t n_allocs = it->second.number_allocations_[i];
      ss << n_allocs << DELIM;
    }
    ss << NEW_LINE;
  }
  std::string output = ss.str();
  return output;
}

void MemoryTrackerCompressedTimeSeriesThread::AcquireSample(
    MemoryTracker* memory_tracker, TimeSeries* timeseries, SbTime time_now) {

  const int sample_count = timeseries->time_stamps_.size();
  timeseries->time_stamps_.push_back(time_now);
  MapAllocationSamples& map_samples = timeseries->samples_;

  std::vector<const AllocationGroup*> vector_output;
  memory_tracker->GetAllocationGroups(&vector_output);

  // Sample all known memory scopes.
  for (int i = 0; i < vector_output.size(); ++i) {
    const AllocationGroup* group = vector_output[i];
    const std::string& name = group->name();

    const bool first_creation = map_samples.find(group->name()) ==
                                map_samples.end();

    AllocationSamples& new_entry  = map_samples[name];

    // Didn't see it before so create new entry.
    if (first_creation) {
      // Make up for lost samples...
      new_entry.allocated_bytes_.resize(sample_count, 0);
      new_entry.number_allocations_.resize(sample_count, 0);
    }

    int32_t num_allocs = - 1;
    int64_t allocation_bytes = -1;
    group->GetAggregateStats(&num_allocs, &allocation_bytes);

    new_entry.allocated_bytes_.push_back(allocation_bytes);
    new_entry.number_allocations_.push_back(num_allocs);
  }
}

bool MemoryTrackerCompressedTimeSeriesThread::IsFull(
    const TimeSeries& timeseries, int samples_limit) {
  SB_DCHECK(samples_limit >= 0);
  return timeseries.time_stamps_.size() >= samples_limit;
}

template<typename VectorT>
void DoCompression(VectorT* samples) {
  for (int i = 0; i*2 < samples->size(); ++i) {
    (*samples)[i] = (*samples)[i*2];
  }
  samples->resize(samples->size() / 2);
}

void MemoryTrackerCompressedTimeSeriesThread::Compress(
    TimeSeries* timeseries) {
  typedef MapAllocationSamples::iterator MapIt;
  MapAllocationSamples& samples = timeseries->samples_;
  DoCompression(&(timeseries->time_stamps_));
  for (MapIt it = samples.begin(); it != samples.end(); ++it) {
    const std::string& name = it->first;
    AllocationSamples& data = it->second;
    DoCompression(&data.allocated_bytes_);
    DoCompression(&data.number_allocations_);
  }
}

void MemoryTrackerCompressedTimeSeriesThread::Print(const std::string& str) {
  SbLogRaw(str.c_str());
  SbLogFlush();
}

}  // namespace analytics
}  // namespace nb

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

#ifndef NB_MEMORY_TRACKER_HELPERS_H_
#define NB_MEMORY_TRACKER_HELPERS_H_

#include <map>
#include <vector>

#include "nb/analytics/memory_tracker.h"
#include "nb/simple_thread.h"
#include "nb/std_allocator.h"
#include "nb/thread_local_boolean.h"
#include "nb/thread_local_pointer.h"
#include "starboard/atomic.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/mutex.h"
#include "starboard/thread.h"
#include "starboard/types.h"

namespace nb {
namespace analytics {

// This is used to build the std allocators for the internal data structures so
// that they don't attempt to report memory allocations, while reporting
// memory allocations.
class NoReportAllocator {
 public:
  NoReportAllocator() {}
  NoReportAllocator(const NoReportAllocator&) {}
  static void* Allocate(size_t n) {
    return SbMemoryAllocateNoReport(n);
  }
  // Second argument can be used for accounting, but is otherwise optional.
  static void Deallocate(void* ptr, size_t /* not used*/) {
    SbMemoryDeallocateNoReport(ptr);
  }
};

class AllocationGroup;
class AllocationRecord;

// An AllocationGroup is a collection of allocations that are logically lumped
// together, such as "Javascript" or "Graphics".
class AllocationGroup {
 public:
  explicit AllocationGroup(const std::string& name);
  ~AllocationGroup();
  const std::string& name() const { return name_; }

  void AddAllocation(int64_t num_bytes);
  void GetAggregateStats(int32_t* num_allocs, int64_t* allocation_bytes) const;

  int64_t allocation_bytes() const;
  int32_t num_allocations() const;

 private:
  const std::string name_;
  starboard::atomic_int64_t allocation_bytes_;
  starboard::atomic_int32_t num_allocations_;

  SB_DISALLOW_COPY_AND_ASSIGN(AllocationGroup);
};

// A self locking data structure that maps strings -> AllocationGroups. This is
// used to resolve MemoryGroup names (e.g. "Javascript") to an AllocationGroup
// which can be used to group allocations together.
class AtomicStringAllocationGroupMap {
 public:
  AtomicStringAllocationGroupMap();
  ~AtomicStringAllocationGroupMap();

  AllocationGroup* Ensure(const std::string& name);
  AllocationGroup* GetDefaultUnaccounted();

  bool Erase(const std::string& key);
  void GetAll(std::vector<const AllocationGroup*>* output) const;

 private:
  // This map bypasses memory reporting of internal memory structures.
  typedef std::map<
      std::string,
      AllocationGroup*,
      std::less<std::string>,
      nb::StdAllocator<
          std::pair<const std::string, AllocationGroup*>,
          NoReportAllocator> > Map;
  Map group_map_;
  AllocationGroup* unaccounted_group_;
  mutable starboard::Mutex mutex_;

  SB_DISALLOW_COPY_AND_ASSIGN(AtomicStringAllocationGroupMap);
};

class AllocationGroupStack {
 public:
  AllocationGroupStack() { Push_DebugBreak(NULL); }
  ~AllocationGroupStack() {}

  void Push(AllocationGroup* group);
  void Pop();
  AllocationGroup* Peek();
  const AllocationGroup* Peek() const;

  void Push_DebugBreak(AllocationGroup* ag) { debug_stack_.push_back(ag); }
  void Pop_DebugBreak() { debug_stack_.pop_back(); }
  AllocationGroup* Peek_DebugBreak() {
    if (debug_stack_.empty()) {
      return NULL;
    }
    return debug_stack_.back();
  }
  const AllocationGroupPtrVec& data() const { return alloc_group_stack_; }

 private:
  SB_DISALLOW_COPY_AND_ASSIGN(AllocationGroupStack);
  AllocationGroupPtrVec alloc_group_stack_, debug_stack_;
};

// A per-pointer map of allocations to AllocRecords. This map is thread safe.
class AtomicAllocationMap {
 public:
  AtomicAllocationMap();
  ~AtomicAllocationMap();

  // Returns true if Added. Otherwise false means that the pointer
  // already existed.
  bool Add(const void* memory, const AllocationRecord& alloc_record);

  // Returns true if the memory exists in this set.
  bool Get(const void* memory, AllocationRecord* alloc_record) const;

  // Return true if the memory existed in this set. If true
  // then output alloc_record is written with record that was found.
  // otherwise the record is written as 0 bytes and null key.
  bool Remove(const void* memory, AllocationRecord* alloc_record);

  bool Accept(AllocationVisitor* visitor) const;

  size_t Size() const;
  bool Empty() const;
  void Clear();

 private:
  // This map bypasses memory reporting of internal memory structures.
  typedef std::map<
      const void*,
      AllocationRecord,
      std::less<const void*>,  // required, when specifying allocator.
      nb::StdAllocator<
          std::pair<const void* const, AllocationRecord>,
          NoReportAllocator> > PointerMap;

  PointerMap pointer_map_;
  mutable starboard::Mutex mutex_;

  SB_DISALLOW_COPY_AND_ASSIGN(AtomicAllocationMap);
};

// A per-pointer map of allocations to AllocRecords. This is a hybrid data
// structure consisting of a hashtable of maps. Each pointer that is
// stored or retrieved is hashed to a random bucket. Each bucket has it's own
// lock. This distributed pattern increases performance significantly by
// reducing contention. The top-level hashtable is of constant size and does
// not resize. Each bucket is implemented as it's own map of elements.
class ConcurrentAllocationMap {
 public:
  static const int kNumElements = 511;
  ConcurrentAllocationMap();
  ~ConcurrentAllocationMap();

  // Returns true if Added. Otherwise false means that the pointer
  // already existed.
  bool Add(const void* memory, const AllocationRecord& alloc_record);
  // Returns true if the memory exists in this set.
  bool Get(const void* memory, AllocationRecord* alloc_record) const;
  // Return true if the memory existed in this set. If true
  // then output alloc_record is written with record that was found.
  // otherwise the record is written as 0 bytes and null key.
  bool Remove(const void* memory, AllocationRecord* alloc_record);
  size_t Size() const;
  bool Empty() const;
  void Clear();

  // Provides access to all the allocations within in a thread safe manner.
  bool Accept(AllocationVisitor* visitor) const;

  AtomicAllocationMap& GetMapForPointer(const void* ptr);
  const AtomicAllocationMap& GetMapForPointer(const void* ptr) const;

 private:
  SB_DISALLOW_COPY_AND_ASSIGN(ConcurrentAllocationMap);
  // Takes a pointer and generates a hash.
  static uint32_t hash_ptr(const void* ptr);

  int ToIndex(const void* ptr) const;
  AtomicAllocationMap pointer_map_array_[kNumElements];
};

}  // namespace analytics
}  // namespace nb

#endif  // NB_MEMORY_TRACKER_HELPERS_H_

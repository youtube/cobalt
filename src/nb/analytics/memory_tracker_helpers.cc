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

#include "nb/analytics/memory_tracker_helpers.h"

#include <stdint.h>
#include <vector>

#include "nb/hash.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"

namespace nb {
namespace analytics {

AllocationGroup::AllocationGroup(const std::string& name)
    : name_(name), allocation_bytes_(0), num_allocations_(0) {
}

AllocationGroup::~AllocationGroup() {}

void AllocationGroup::AddAllocation(int64_t num_bytes) {
  if (num_bytes == 0)
    return;
  int num_alloc_diff = num_bytes > 0 ? 1 : -1;
  allocation_bytes_.fetch_add(num_bytes);
  num_allocations_.fetch_add(num_alloc_diff);
}

void AllocationGroup::GetAggregateStats(int32_t* num_allocs,
                                        int64_t* allocation_bytes) const {
  *num_allocs = num_allocations_.load();
  *allocation_bytes = allocation_bytes_.load();
}

int64_t AllocationGroup::allocation_bytes() const {
  return allocation_bytes_.load();
}

int32_t AllocationGroup::num_allocations() const {
  return num_allocations_.load();
}

AtomicStringAllocationGroupMap::AtomicStringAllocationGroupMap() {
  unaccounted_group_ = Ensure("Unaccounted");
}

AtomicStringAllocationGroupMap::~AtomicStringAllocationGroupMap() {
  unaccounted_group_ = NULL;
  while (!group_map_.empty()) {
    Map::iterator it = group_map_.begin();
    delete it->second;
    group_map_.erase(it);
  }
}

AllocationGroup* AtomicStringAllocationGroupMap::Ensure(
    const std::string& name) {
  starboard::ScopedLock lock(mutex_);
  Map::const_iterator found_it = group_map_.find(name);
  if (found_it != group_map_.end()) {
    return found_it->second;
  }
  AllocationGroup* group = new AllocationGroup(name);
  group_map_[name] = group;
  return group;
}

AllocationGroup* AtomicStringAllocationGroupMap::GetDefaultUnaccounted() {
  return unaccounted_group_;
}

bool AtomicStringAllocationGroupMap::Erase(const std::string& name) {
  starboard::ScopedLock lock(mutex_);
  Map::iterator found_it = group_map_.find(name);
  if (found_it == group_map_.end()) {
    // Didn't find it.
    return false;
  }
  group_map_.erase(found_it);
  return true;
}

void AtomicStringAllocationGroupMap::GetAll(
    std::vector<const AllocationGroup*>* output) const {
  starboard::ScopedLock lock(mutex_);
  for (Map::const_iterator it = group_map_.begin(); it != group_map_.end();
       ++it) {
    output->push_back(it->second);
  }
}

void AllocationGroupStack::Push(AllocationGroup* group) {
  alloc_group_stack_.push_back(group);
}

void AllocationGroupStack::Pop() {
  alloc_group_stack_.pop_back();
}

AllocationGroup* AllocationGroupStack::Peek() {
  if (alloc_group_stack_.empty()) {
    return NULL;
  }
  return alloc_group_stack_.back();
}

const AllocationGroup* AllocationGroupStack::Peek() const {
  if (alloc_group_stack_.empty()) {
    return NULL;
  }
  return alloc_group_stack_.back();
}

AtomicAllocationMap::AtomicAllocationMap() {}

AtomicAllocationMap::~AtomicAllocationMap() {}

bool AtomicAllocationMap::Add(const void* memory,
                              const AllocationRecord& alloc_record) {
  starboard::ScopedLock lock(mutex_);
  const bool inserted =
      pointer_map_.insert(std::make_pair(memory, alloc_record)).second;
  return inserted;
}

bool AtomicAllocationMap::Get(const void* memory,
                              AllocationRecord* alloc_record) const {
  starboard::ScopedLock lock(mutex_);
  PointerMap::const_iterator found_it = pointer_map_.find(memory);
  if (found_it == pointer_map_.end()) {
    if (alloc_record) {
      *alloc_record = AllocationRecord::Empty();
    }
    return false;
  }
  if (alloc_record) {
    *alloc_record = found_it->second;
  }
  return true;
}

bool AtomicAllocationMap::Remove(const void* memory,
                                 AllocationRecord* alloc_record) {
  starboard::ScopedLock lock(mutex_);
  PointerMap::iterator found_it = pointer_map_.find(memory);

  if (found_it == pointer_map_.end()) {
    if (alloc_record) {
      *alloc_record = AllocationRecord::Empty();
    }
    return false;
  }
  if (alloc_record) {
    *alloc_record = found_it->second;
  }
  pointer_map_.erase(found_it);
  return true;
}

bool AtomicAllocationMap::Accept(AllocationVisitor* visitor) const {
  starboard::ScopedLock lock(mutex_);
  for (PointerMap::const_iterator it = pointer_map_.begin();
       it != pointer_map_.end(); ++it) {
    const void* memory = it->first;
    const AllocationRecord& alloc_rec = it->second;
    if (!visitor->Visit(memory, alloc_rec)) {
      return false;
    }
  }
  return true;
}

size_t AtomicAllocationMap::Size() const {
  starboard::ScopedLock lock(mutex_);
  return pointer_map_.size();
}

bool AtomicAllocationMap::Empty() const {
  starboard::ScopedLock lock(mutex_);
  return pointer_map_.empty();
}

void AtomicAllocationMap::Clear() {
  starboard::ScopedLock lock(mutex_);
  for (PointerMap::iterator it = pointer_map_.begin();
       it != pointer_map_.end(); ++it) {
    const AllocationRecord& rec = it->second;
    AllocationGroup* group = rec.allocation_group;
    const int64_t size = static_cast<int64_t>(rec.size);
    group->AddAllocation(-size);
  }
  return pointer_map_.clear();
}

ConcurrentAllocationMap::ConcurrentAllocationMap() : pointer_map_array_() {}

ConcurrentAllocationMap::~ConcurrentAllocationMap() {
  Clear();
}

bool ConcurrentAllocationMap::Add(const void* memory,
                                  const AllocationRecord& alloc_record) {
  AtomicAllocationMap& map = GetMapForPointer(memory);
  return map.Add(memory, alloc_record);
}

bool ConcurrentAllocationMap::Get(const void* memory,
                                  AllocationRecord* alloc_record) const {
  const AtomicAllocationMap& map = GetMapForPointer(memory);
  return map.Get(memory, alloc_record);
}

bool ConcurrentAllocationMap::Remove(const void* memory,
                                     AllocationRecord* alloc_record) {
  AtomicAllocationMap& map = GetMapForPointer(memory);
  bool output = map.Remove(memory, alloc_record);
  return output;
}

size_t ConcurrentAllocationMap::Size() const {
  size_t size = 0;
  for (int i = 0; i < kNumElements; ++i) {
    const AtomicAllocationMap& map = pointer_map_array_[i];
    size += map.Size();
  }
  return size;
}

bool ConcurrentAllocationMap::Empty() const {
  return 0 == Size();
}

void ConcurrentAllocationMap::Clear() {
  for (int i = 0; i < kNumElements; ++i) {
    AtomicAllocationMap& map = pointer_map_array_[i];
    map.Clear();
  }
}

bool ConcurrentAllocationMap::Accept(AllocationVisitor* visitor) const {
  for (int i = 0; i < kNumElements; ++i) {
    const AtomicAllocationMap& map = pointer_map_array_[i];
    if (!map.Accept(visitor)) {
      return false;  // Early out.
    }
  }
  return true;
}

uint32_t ConcurrentAllocationMap::hash_ptr(const void* ptr) {
  uintptr_t val = reinterpret_cast<uintptr_t>(ptr);
  return RuntimeHash32(reinterpret_cast<const char*>(&val), sizeof(val));
}

int ConcurrentAllocationMap::ToIndex(const void* ptr) const {
  SB_DCHECK(0 != kNumElements);
  uint32_t hash_val = hash_ptr(ptr);
  int index = hash_val % kNumElements;
  return index;
}

AtomicAllocationMap& ConcurrentAllocationMap::GetMapForPointer(
    const void* ptr) {
  return pointer_map_array_[ToIndex(ptr)];
}

const AtomicAllocationMap& ConcurrentAllocationMap::GetMapForPointer(
    const void* ptr) const {
  return pointer_map_array_[ToIndex(ptr)];
}

}  // namespace analytics
}  // namespace nb

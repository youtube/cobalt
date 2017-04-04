// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "starboard/shared/starboard/thread_local_storage_internal.h"

#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/once.h"

struct SbThreadLocalKeyPrivate {
  int index;
};

namespace starboard {
namespace shared {

namespace {
TLSKeyManager* s_instance = NULL;
SbOnceControl s_instance_control = SB_ONCE_INITIALIZER;

void InitializeTLS() {
  s_instance = new TLSKeyManager();
}

// RawAllocator uses the SbMemoryAllocateNoReport() and
// SbMemoryDeallocateNoReport() allocation functions. This allows the
// TLSKeyManager to be used with the memory tracking system.
// Without this allocator, the TLSKeyManager could receive an event for memory
// allocation, which will cause memory to be allocated in the map, which will
// generate an event for memory allocation ... which results in an infinite
// loop and a crash.
template <typename T>
class RawAllocator : public std::allocator<T> {
 public:
  typedef typename std::allocator<T>::pointer pointer;
  typedef typename std::allocator<T>::const_pointer const_pointer;
  typedef typename std::allocator<T>::reference reference;
  typedef typename std::allocator<T>::const_reference const_reference;
  typedef typename std::allocator<T>::size_type size_type;
  typedef typename std::allocator<T>::value_type value_type;
  typedef typename std::allocator<T>::difference_type difference_type;

  RawAllocator() {}

  // Constructor used for rebinding
  template <typename U>
  RawAllocator(const RawAllocator<U>& x) {}

  pointer allocate(size_type n,
                   std::allocator<void>::const_pointer hint = NULL) {
    void* ptr = SbMemoryAllocateNoReport(n * sizeof(value_type));
    return static_cast<pointer>(ptr);
  }

  void deallocate(pointer p, size_type n) { SbMemoryDeallocateNoReport(p); }
  template <typename U>
  struct rebind {
    typedef RawAllocator<U> other;
  };
};

}  // namespace

struct TLSKeyManager::InternalData {
  // These data structures bypass memory reporting. If this wasn't here then
  // any platform using this TLSKeyManager will crash during memory reporting.
  typedef std::vector<KeyRecord, RawAllocator<KeyRecord> > VectorKeyRecord;
  typedef std::vector<int, RawAllocator<int> > VectorInt;
  typedef std::map<SbThreadId,
                   int,
                   std::less<SbThreadId>,
                   RawAllocator<std::pair<const SbThreadId, int> > >
      ThreadIdMap;

  // Overrides new/delete for InternalData to bypass memory reporting.
  static void* operator new(size_t n) { return SbMemoryAllocateNoReport(n); }
  static void operator delete(void* p) { SbMemoryDeallocateNoReport(p); }

  // The key record tracks all key values among all threads, along with their
  // destructors, if specified.
  VectorKeyRecord key_table_;

  // Tracks all thread IDs that are still available.
  VectorInt available_thread_ids_;

  // This maps Starboard thread IDs to TLS thread ids.
  ThreadIdMap thread_id_map_;
};

TLSKeyManager* TLSKeyManager::Get() {
  SbOnce(&s_instance_control, &InitializeTLS);
  return s_instance;
}

TLSKeyManager::TLSKeyManager() {
  data_.reset(new InternalData);
  data_->available_thread_ids_.reserve(kMaxThreads);
  for (int i = 0; i < kMaxThreads; ++i) {
    data_->available_thread_ids_.push_back(i);
  }
}

SbThreadLocalKey TLSKeyManager::CreateKey(SbThreadLocalDestructor destructor) {
  void* memory_block =
      SbMemoryAllocateNoReport(sizeof(SbThreadLocalKeyPrivate));
  SbThreadLocalKey key = new(memory_block) SbThreadLocalKeyPrivate();

  ScopedLock lock(mutex_);
  key->index = GetUnusedKeyIndex();

  KeyRecord* record = &data_->key_table_[key->index];

  record->destructor = destructor;
  SbMemorySet(record->values, 0, sizeof(record->values));
  record->valid = true;

  return key;
}

void TLSKeyManager::DestroyKey(SbThreadLocalKey key) {
  if (!SbThreadIsValidLocalKey(key)) {
    return;
  }

  ScopedLock lock(mutex_);

  SB_DCHECK(IsKeyActive(key));
  data_->key_table_[key->index].valid = false;

  key.~SbThreadLocalKey();
  SbMemoryDeallocateNoReport(key);
}

bool TLSKeyManager::SetLocalValue(SbThreadLocalKey key, void* value) {
  if (!SbThreadIsValidLocalKey(key)) {
    return false;
  }

  ScopedLock lock(mutex_);

  if (!IsKeyActive(key)) {
    return false;
  }

  data_->key_table_[key->index].values[GetCurrentThreadId()] = value;

  return true;
}

void* TLSKeyManager::GetLocalValue(SbThreadLocalKey key) {
  if (!SbThreadIsValidLocalKey(key)) {
    return NULL;
  }

  ScopedLock lock(mutex_);

  if (!IsKeyActive(key)) {
    return NULL;
  }

  return data_->key_table_[key->index].values[GetCurrentThreadId()];
}

void TLSKeyManager::InitializeTLSForThread() {
  ScopedLock lock(mutex_);

  int current_thread_id = GetCurrentThreadId();

  const size_t table_size = data_->key_table_.size();
  for (int i = 0; i < table_size; ++i) {
    KeyRecord* key_record = &data_->key_table_[i];
    if (key_record->valid) {
      key_record->values[current_thread_id] = NULL;
    }
  }
}

void TLSKeyManager::ShutdownTLSForThread() {
  ScopedLock lock(mutex_);

  int current_thread_id = GetCurrentThreadId();

  // Apply the destructors multiple times (4 is the minimum value
  // according to the specifications).  This is necessary if one of
  // the destructors adds new values to the key map.
  for (int d = 0; d < 4; ++d) {
    // Move the map into a new temporary map so that we can iterate
    // through that while the original s_tls_thread_keys may have more
    // values added to it via destructor calls.
    const size_t table_size = data_->key_table_.size();

    for (int i = 0; i < table_size; ++i) {
      KeyRecord* key_record = &data_->key_table_[i];
      if (key_record->valid) {
        void* value = key_record->values[current_thread_id];
        key_record->values[current_thread_id] = NULL;
        SbThreadLocalDestructor destructor = key_record->destructor;

        if (value && destructor) {
          mutex_.Release();
          destructor(value);
          mutex_.Acquire();
        }
      }
    }
  }

  data_->thread_id_map_.erase(SbThreadGetId());
  data_->available_thread_ids_.push_back(current_thread_id);
}

bool TLSKeyManager::IsKeyActive(SbThreadLocalKey key) {
  return data_->key_table_[key->index].valid;
}

int TLSKeyManager::GetUnusedKeyIndex() {
  const size_t key_table_size = data_->key_table_.size();
  for (int i = 0; i < key_table_size; ++i) {
    if (!data_->key_table_[i].valid) {
      return i;
    }
  }

  data_->key_table_.push_back(KeyRecord());
  data_->key_table_.back().valid = false;

  return data_->key_table_.size() - 1;
}

int TLSKeyManager::GetCurrentThreadId() {
  InternalData::ThreadIdMap::const_iterator found =
      data_->thread_id_map_.find(SbThreadGetId());
  if (found != data_->thread_id_map_.end()) {
    return found->second;
  }

  SB_DCHECK(!data_->available_thread_ids_.empty());
  int thread_tls_id = data_->available_thread_ids_.back();
  data_->available_thread_ids_.pop_back();
  data_->thread_id_map_.insert(std::make_pair(SbThreadGetId(), thread_tls_id));

  return thread_tls_id;
}

}  // namespace shared
}  // namespace starboard

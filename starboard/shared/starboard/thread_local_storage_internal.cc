// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/flat_map.h"
#include "starboard/common/log.h"
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

// A map of ThreadId -> int. This map is highly concurrent and allows
// access of elements without much contention.
class ConcurrentThreadIdMap {
 public:
  ConcurrentThreadIdMap() {
    // Prime number reduces collisions.
    static const size_t kNumBuckets = 101;
    map_vector_.resize(kNumBuckets);

    for (size_t i = 0; i < map_vector_.size(); ++i) {
      void* memory_block = SbMemoryAllocateNoReport(sizeof(LockedMap));
      map_vector_[i] = new (memory_block) LockedMap;
    }
  }

  ~ConcurrentThreadIdMap() {
    for (size_t i = 0; i < map_vector_.size(); ++i) {
      LockedMap* obj = map_vector_[i];
      obj->~LockedMap();
      SbMemoryDeallocateNoReport(obj);
    }
  }

  bool GetIfExists(SbThreadId key, int* value) const {
    const LockedMap& map = GetBucket(key);
    ScopedLock lock(map.mutex_);
    ThreadIdMap::const_iterator it = map.map_.find(key);
    if (it != map.map_.end()) {
      *value = it->second;
      return true;
    } else {
      return false;
    }
  }

  void Insert(SbThreadId key, int value) {
    LockedMap& map = GetBucket(key);
    ScopedLock lock(map.mutex_);
    map.map_[key] = value;
  }

  void Erase(SbThreadId key) {
    LockedMap& map = GetBucket(key);
    ScopedLock lock(map.mutex_);
    map.map_.erase(key);
  }

 private:
  typedef std::map<SbThreadId,
                   int,
                   std::less<SbThreadId>,
                   RawAllocator<std::pair<const SbThreadId, int> > >
      ThreadIdMap;

  struct LockedMap {
    Mutex mutex_;
    ThreadIdMap map_;
  };

  // Simple hashing function for 32 bit numbers.
  // Based off of Jenkins hash found at this url:
  // https://gist.github.com/badboy/6267743#file-inthash-md
  static size_t Hash(SbThreadId id) {
    static const uint32_t kMagicNum1 = 0x7ed55d16;
    static const uint32_t kMagicNum2 = 0xc761c23c;
    static const uint32_t kMagicNum3 = 0x165667b1;
    static const uint32_t kMagicNum4 = 0xd3a2646c;
    static const uint32_t kMagicNum5 = 0xfd7046c5;
    static const uint32_t kMagicNum6 = 0xfd7046c5;

    static const uint32_t kMagicShift1 = 12;
    static const uint32_t kMagicShift2 = 19;
    static const uint32_t kMagicShift3 = 5;
    static const uint32_t kMagicShift4 = 9;
    static const uint32_t kMagicShift5 = 3;
    static const uint32_t kMagicShift6 = 16;

    uint32_t key = static_cast<uint32_t>(id);
    key = (key + kMagicNum1) + (key << kMagicShift1);
    key = (key ^ kMagicNum2) ^ (key >> kMagicShift2);
    key = (key + kMagicNum3) + (key << kMagicShift3);
    key = (key + kMagicNum4) ^ (key << kMagicShift4);
    key = (key + kMagicNum5) + (key << kMagicShift5);
    key = (key ^ kMagicNum6) ^ (key >> kMagicShift6);
    return static_cast<size_t>(key);
  }

  const LockedMap& GetBucket(SbThreadId key) const {
    size_t bucket_index = Hash(key) % map_vector_.size();
    return *map_vector_[bucket_index];
  }

  LockedMap& GetBucket(SbThreadId key) {
    size_t bucket_index = Hash(key) % map_vector_.size();
    return *map_vector_[bucket_index];
  }

  typedef std::vector<LockedMap*, RawAllocator<LockedMap*> > MapVector;
  MapVector map_vector_;
};

}  // namespace

struct TLSKeyManager::InternalData {
  // These data structures bypass memory reporting. If this wasn't here then
  // any platform using this TLSKeyManager will crash during memory reporting.
  typedef std::vector<KeyRecord, RawAllocator<KeyRecord> > VectorKeyRecord;
  typedef std::vector<int, RawAllocator<int> > VectorInt;
  typedef ConcurrentThreadIdMap ThreadIdMap;

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
  // Allocate key and bypass the the normal allocator. Otherwise there
  // could be a re-entrant loop that kills the process.
  void* memory_block =
      SbMemoryAllocateNoReport(sizeof(SbThreadLocalKeyPrivate));
  SbThreadLocalKey key = new (memory_block) SbThreadLocalKeyPrivate();

  ScopedLock lock(mutex_);
  key->index = GetUnusedKeyIndex();

  KeyRecord* record = &data_->key_table_[key->index];

  record->destructor = destructor;
#if SB_API_VERSION >= 12
  memset(record->values.data(), 0,
              record->values.size() * sizeof(record->values[0]));
#else   // SB_API_VERSION >= 12
  memset(record->values, 0, sizeof(record->values));
#endif  // SB_API_VERSION >= 12
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

  int current_thread_id = GetCurrentThreadId();

  ScopedLock lock(mutex_);

  if (!IsKeyActive(key)) {
    return false;
  }

  data_->key_table_[key->index].values[current_thread_id] = value;

  return true;
}

void* TLSKeyManager::GetLocalValue(SbThreadLocalKey key) {
  if (!SbThreadIsValidLocalKey(key)) {
    return NULL;
  }

  int current_thread_id = GetCurrentThreadId();

  ScopedLock lock(mutex_);

  if (!IsKeyActive(key)) {
    return NULL;
  }

  return data_->key_table_[key->index].values[current_thread_id];
}

void TLSKeyManager::InitializeTLSForThread() {
  int current_thread_id = GetCurrentThreadId();

  ScopedLock lock(mutex_);

  const size_t table_size = data_->key_table_.size();
  for (int i = 0; i < table_size; ++i) {
    KeyRecord* key_record = &data_->key_table_[i];
    if (key_record->valid) {
      key_record->values[current_thread_id] = NULL;
    }
  }
}

void TLSKeyManager::ShutdownTLSForThread() {
  int current_thread_id = GetCurrentThreadId();

  ScopedLock lock(mutex_);

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

  data_->thread_id_map_.Erase(SbThreadGetId());
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
  const SbThreadId thread_id = SbThreadGetId();

  int value = -1;
  if (data_->thread_id_map_.GetIfExists(thread_id, &value)) {
    return value;
  }

  ScopedLock lock(mutex_);

  SB_DCHECK(!data_->available_thread_ids_.empty());
  int thread_tls_id = data_->available_thread_ids_.back();
  data_->available_thread_ids_.pop_back();

  data_->thread_id_map_.Insert(thread_id, thread_tls_id);
  return thread_tls_id;
}

}  // namespace shared
}  // namespace starboard

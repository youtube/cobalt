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

#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/once.h"
#include "starboard/shared/starboard/thread_local_storage_internal.h"

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

}  // namespace

TLSKeyManager* TLSKeyManager::Get() {
  SbOnce(&s_instance_control, &InitializeTLS);
  return s_instance;
}

TLSKeyManager::TLSKeyManager() {
  available_thread_ids_.reserve(kMaxThreads);
  for (int i = 0; i < kMaxThreads; ++i) {
    available_thread_ids_.push_back(i);
  }
}

SbThreadLocalKey TLSKeyManager::CreateKey(SbThreadLocalDestructor destructor) {
  SbThreadLocalKey key = new SbThreadLocalKeyPrivate();

  ScopedLock lock(mutex_);
  key->index = GetUnusedKeyIndex();

  KeyRecord* record = &key_table_[key->index];

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
  key_table_[key->index].valid = false;

  delete key;
}

bool TLSKeyManager::SetLocalValue(SbThreadLocalKey key, void* value) {
  if (!SbThreadIsValidLocalKey(key)) {
    return false;
  }

  ScopedLock lock(mutex_);

  if (!IsKeyActive(key)) {
    return false;
  }

  key_table_[key->index].values[GetCurrentThreadId()] = value;

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

  return key_table_[key->index].values[GetCurrentThreadId()];
}

void TLSKeyManager::InitializeTLSForThread() {
  ScopedLock lock(mutex_);

  int current_thread_id = GetCurrentThreadId();

  for (int i = 0; i < key_table_.size(); ++i) {
    KeyRecord* key_record = &key_table_[i];
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
    for (int i = 0; i < key_table_.size(); ++i) {
      KeyRecord* key_record = &key_table_[i];
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

  thread_id_map_.erase(SbThreadGetId());
  available_thread_ids_.push_back(current_thread_id);
}

bool TLSKeyManager::IsKeyActive(SbThreadLocalKey key) {
  return key_table_[key->index].valid;
}

int TLSKeyManager::GetUnusedKeyIndex() {
  for (int i = 0; i < key_table_.size(); ++i) {
    if (!key_table_[i].valid) {
      return i;
    }
  }

  key_table_.push_back(KeyRecord());
  key_table_.back().valid = false;

  return key_table_.size() - 1;
}

int TLSKeyManager::GetCurrentThreadId() {
  std::map<SbThreadId, int>::const_iterator found =
      thread_id_map_.find(SbThreadGetId());
  if (found != thread_id_map_.end()) {
    return found->second;
  }

  SB_DCHECK(!available_thread_ids_.empty());
  int thread_tls_id = available_thread_ids_.back();
  available_thread_ids_.pop_back();

  thread_id_map_.insert(std::make_pair(SbThreadGetId(), thread_tls_id));

  return thread_tls_id;
}

}  // namespace shared
}  // namespace starboard

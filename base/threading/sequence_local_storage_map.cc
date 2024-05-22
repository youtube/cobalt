// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequence_local_storage_map.h"

#include <ostream>
#include <utility>

#include "base/check_op.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

#if defined(STARBOARD)
#include <pthread.h>

#include "base/check_op.h"
#include "starboard/thread.h"
#endif

namespace base {
namespace internal {

namespace {

#if defined(STARBOARD)
ABSL_CONST_INIT pthread_once_t s_once_flag = PTHREAD_ONCE_INIT;
ABSL_CONST_INIT pthread_key_t s_thread_local_key = 0;

void InitThreadLocalKey() {
  int res = pthread_key_create(&s_thread_local_key , NULL);
  DCHECK(res == 0);
}

void EnsureThreadLocalKeyInited() {
  pthread_once(&s_once_flag, InitThreadLocalKey);
}

SequenceLocalStorageMap* GetCurrentSequenceLocalStorage() {
  EnsureThreadLocalKeyInited();
  return static_cast<SequenceLocalStorageMap*>(
      pthread_getspecific(s_thread_local_key));
}
#else
ABSL_CONST_INIT thread_local SequenceLocalStorageMap*
    current_sequence_local_storage = nullptr;
#endif

}  // namespace

SequenceLocalStorageMap::SequenceLocalStorageMap() = default;

SequenceLocalStorageMap::~SequenceLocalStorageMap() = default;

// static
SequenceLocalStorageMap& SequenceLocalStorageMap::GetForCurrentThread() {
  DCHECK(IsSetForCurrentThread())
      << "SequenceLocalStorageSlot cannot be used because no "
         "SequenceLocalStorageMap was stored in TLS. Use "
         "ScopedSetSequenceLocalStorageMapForCurrentThread to store a "
         "SequenceLocalStorageMap object in TLS.";

#if defined(STARBOARD)
  return *GetCurrentSequenceLocalStorage();
#else
  return *current_sequence_local_storage;
#endif
}

// static
bool SequenceLocalStorageMap::IsSetForCurrentThread() {
#if defined(STARBOARD)
  return GetCurrentSequenceLocalStorage() != nullptr;
#else
  return current_sequence_local_storage != nullptr;
#endif
}

void* SequenceLocalStorageMap::Get(int slot_id) {
  const auto it = sls_map_.find(slot_id);
  if (it == sls_map_.end())
    return nullptr;
  return it->second.value();
}

void SequenceLocalStorageMap::Set(
    int slot_id,
    SequenceLocalStorageMap::ValueDestructorPair value_destructor_pair) {
  auto it = sls_map_.find(slot_id);

  if (it == sls_map_.end())
    sls_map_.emplace(slot_id, std::move(value_destructor_pair));
  else
    it->second = std::move(value_destructor_pair);

  // The maximum number of entries in the map is 256. This can be adjusted, but
  // will require reviewing the choice of data structure for the map.
  DCHECK_LE(sls_map_.size(), 256U);
}

SequenceLocalStorageMap::ValueDestructorPair::ValueDestructorPair(
    void* value,
    DestructorFunc* destructor)
    : value_(value), destructor_(destructor) {}

SequenceLocalStorageMap::ValueDestructorPair::~ValueDestructorPair() {
  if (value_)
    destructor_(value_);
}

SequenceLocalStorageMap::ValueDestructorPair::ValueDestructorPair(
    ValueDestructorPair&& value_destructor_pair)
    : value_(value_destructor_pair.value_),
      destructor_(value_destructor_pair.destructor_) {
  value_destructor_pair.value_ = nullptr;
}

SequenceLocalStorageMap::ValueDestructorPair&
SequenceLocalStorageMap::ValueDestructorPair::operator=(
    ValueDestructorPair&& value_destructor_pair) {
  // Destroy |value_| before overwriting it with a new value.
  if (value_)
    destructor_(value_);

  value_ = value_destructor_pair.value_;
  destructor_ = value_destructor_pair.destructor_;

  value_destructor_pair.value_ = nullptr;

  return *this;
}

ScopedSetSequenceLocalStorageMapForCurrentThread::
    ScopedSetSequenceLocalStorageMapForCurrentThread(
        SequenceLocalStorageMap* sequence_local_storage)
#if defined(STARBOARD)
{
  EnsureThreadLocalKeyInited();
  pthread_setspecific(s_thread_local_key, sequence_local_storage);
}
#else
    : resetter_(&current_sequence_local_storage,
                sequence_local_storage,
                nullptr) {}
#endif

#if defined(STARBOARD)
ScopedSetSequenceLocalStorageMapForCurrentThread::
    ~ScopedSetSequenceLocalStorageMapForCurrentThread() {
  EnsureThreadLocalKeyInited();
  pthread_setspecific(s_thread_local_key, nullptr);
}
#else
ScopedSetSequenceLocalStorageMapForCurrentThread::
    ~ScopedSetSequenceLocalStorageMapForCurrentThread() = default;
#endif

}  // namespace internal
}  // namespace base

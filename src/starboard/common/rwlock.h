// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Mutex module
//
// Defines a read-write lock that allows concurrent read operations but
// exclusive access for writes.

#ifndef STARBOARD_COMMON_RWLOCK_H_
#define STARBOARD_COMMON_RWLOCK_H_

#include "starboard/condition_variable.h"
#include "starboard/mutex.h"

namespace starboard {

// This RWLock allows concurrent access for read-only operations, while write
// operations require exclusive access. This means that multiple threads can
// read the data in parallel but an exclusive lock is needed for writing or
// modifying data.
//
// This RWLock is non-upgradeable, if the read lock is held then it must
// be released before the write lock can be used.
//
// This implementation favors writers over readers.
//
// Example:
//   std::map<...> my_map = ...;
//
//   RWLock rw_lock;
//
//   bool Exists() {                     // This function is concurrent.
//     rw_lock.AcquireReadLock();
//     bool found = my_map.find(...) != my_map.end();
//     rw_lock.ReleaseReadLock();
//     return found;
//   }
//   void Add(...) {                     // Write operations are exclusive.
//     rw_lock.AcquireWriteLock();
//     my_map.insert(...);
//     rw_lock.ReleaseWriteLock();
//   }
class RWLock {
 public:
  RWLock();
  ~RWLock();

  void AcquireReadLock();
  void ReleaseReadLock();

  void AcquireWriteLock();
  void ReleaseWriteLock();

 private:
  SbMutex mutex_;
  SbConditionVariable condition_;
  int32_t readers_;
  bool writing_;

  RWLock(const RWLock&) = delete;
  void operator=(const RWLock&) = delete;
};

class ScopedReadLock {
 public:
  explicit ScopedReadLock(RWLock* rw_lock) : rw_lock_(rw_lock) {
    rw_lock_->AcquireReadLock();
  }
  ~ScopedReadLock() { rw_lock_->ReleaseReadLock(); }

 private:
  RWLock* rw_lock_;

  ScopedReadLock(const ScopedReadLock&) = delete;
  void operator=(const ScopedReadLock&) = delete;
};

class ScopedWriteLock {
 public:
  explicit ScopedWriteLock(RWLock* rw_lock) : rw_lock_(rw_lock) {
    rw_lock_->AcquireWriteLock();
  }
  ~ScopedWriteLock() { rw_lock_->ReleaseWriteLock(); }

 private:
  RWLock* rw_lock_;

  ScopedWriteLock(const ScopedWriteLock&) = delete;
  void operator=(const ScopedWriteLock&) = delete;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_RWLOCK_H_

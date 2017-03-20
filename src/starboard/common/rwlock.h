// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/common/semaphore.h"
#include "starboard/mutex.h"

#ifdef __cplusplus
namespace starboard {

// This RWLock allows concurrent access for read-only operations, while write
// operations require exclusive access. This means that multiple threads can
// read the data in parallel but an exclusive lock is needed for writing or
// modifying data.
//
// This RWLock is non-upgradeable, if the read lock is held then it must
// be released before the write lock can be used.
//
// This implementation favors readers over writers. This implementation was
// first described by Michel Raynal.
//
// Example:
//   std::map<...> my_map = ...;
//
//   RWLock rw_lock;
//
//   bool Exists() {                     // This function is concurrent.
//     rw_lock.AcquireReadLock();
//     bool found = my_map.find(...);
//     rw_lock.ReleaseReadLock();
//     return found;
//   }
//   bool Add(...) {                     // Write operations are exclusive.
//     rw_lock.AcquireWriteLock();
//     bool erased = my_map.erase(...);
//     rw_lock.ReleaseWriteLock();
//     return erased;
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
  Mutex reader_;
  // Semaphore is necessary because of shared ownership: a different
  // thread can unlock a writer than the one that locks it.
  Semaphore writer_;
  int num_readers_;
  SB_DISALLOW_COPY_AND_ASSIGN(RWLock);
};

class ScopedReadLock {
 public:
  explicit ScopedReadLock(RWLock* rw_lock);
  ~ScopedReadLock();

 private:
  RWLock* rw_lock_;
  SB_DISALLOW_COPY_AND_ASSIGN(ScopedReadLock);
};

class ScopedWriteLock {
 public:
  explicit ScopedWriteLock(RWLock* rw_lock);
  ~ScopedWriteLock();

 private:
  RWLock* rw_lock_;
  SB_DISALLOW_COPY_AND_ASSIGN(ScopedWriteLock);
};

/////////////////////////////////// IMPL //////////////////////////////////////

inline RWLock::RWLock() : num_readers_(0), reader_(), writer_(1) {}

inline RWLock::~RWLock() {}

inline void RWLock::AcquireReadLock() {
  reader_.Acquire();
  if (0 == num_readers_++) {
    AcquireWriteLock();
  }
  reader_.Release();
}

inline void RWLock::ReleaseReadLock() {
  reader_.Acquire();
  if (--num_readers_ == 0) {
    ReleaseWriteLock();
  }
  reader_.Release();
}

inline void RWLock::AcquireWriteLock() {
  writer_.Take();
}

inline void RWLock::ReleaseWriteLock() {
  writer_.Put();
}

inline ScopedReadLock::ScopedReadLock(RWLock* rw_lock) : rw_lock_(rw_lock) {
  rw_lock_->AcquireReadLock();
}

inline ScopedReadLock::~ScopedReadLock() {
  rw_lock_->ReleaseReadLock();
}

inline ScopedWriteLock::ScopedWriteLock(RWLock* rw_lock) : rw_lock_(rw_lock) {
  rw_lock_->AcquireWriteLock();
}

inline ScopedWriteLock::~ScopedWriteLock() {
  rw_lock_->ReleaseWriteLock();
}

}  // namespace starboard
#endif  // __cplusplus

#endif  // STARBOARD_COMMON_RWLOCK_H_

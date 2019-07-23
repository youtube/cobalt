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

#include "starboard/common/rwlock.h"

namespace starboard {

RWLock::RWLock() : num_readers_(0), reader_(), writer_(1) {}

RWLock::~RWLock() {}

void RWLock::AcquireReadLock() {
  reader_.Acquire();
  if (0 == num_readers_++) {
    AcquireWriteLock();
  }
  reader_.Release();
}

void RWLock::ReleaseReadLock() {
  reader_.Acquire();
  if (--num_readers_ == 0) {
    ReleaseWriteLock();
  }
  reader_.Release();
}

void RWLock::AcquireWriteLock() {
  writer_.Take();
}

void RWLock::ReleaseWriteLock() {
  writer_.Put();
}

ScopedReadLock::ScopedReadLock(RWLock* rw_lock) : rw_lock_(rw_lock) {
  rw_lock_->AcquireReadLock();
}

ScopedReadLock::~ScopedReadLock() {
  rw_lock_->ReleaseReadLock();
}

ScopedWriteLock::ScopedWriteLock(RWLock* rw_lock) : rw_lock_(rw_lock) {
  rw_lock_->AcquireWriteLock();
}

ScopedWriteLock::~ScopedWriteLock() {
  rw_lock_->ReleaseWriteLock();
}

}  // namespace starboard

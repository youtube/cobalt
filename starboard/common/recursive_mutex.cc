// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/recursive_mutex.h"
#include "starboard/common/log.h"
#include "starboard/time.h"

namespace starboard {

RecursiveMutex::RecursiveMutex()
    : owner_id_(kSbThreadInvalidId), recurse_count_(0) {}

RecursiveMutex::~RecursiveMutex() {}

void RecursiveMutex::Acquire() {
  SbThreadId current_thread = SbThreadGetId();
  if (owner_id_ == current_thread) {
    recurse_count_++;
    SB_DCHECK(recurse_count_ > 0);
    return;
  }
  mutex_.Acquire();
  owner_id_ = current_thread;
  recurse_count_ = 1;
}

void RecursiveMutex::Release() {
  SB_DCHECK(owner_id_ == SbThreadGetId());
  if (owner_id_ == SbThreadGetId()) {
    SB_DCHECK(0 < recurse_count_);
    recurse_count_--;
    if (recurse_count_ == 0) {
      owner_id_ = kSbThreadInvalidId;
      mutex_.Release();
    }
  }
}

bool RecursiveMutex::AcquireTry() {
  SbThreadId current_thread = SbThreadGetId();
  if (owner_id_ == current_thread) {
    recurse_count_++;
    SB_DCHECK(recurse_count_ > 0);
    return true;
  }
  if (!mutex_.AcquireTry()) {
    return false;
  }
  owner_id_ = current_thread;
  recurse_count_ = 1;
  return true;
}

ScopedRecursiveLock::ScopedRecursiveLock(RecursiveMutex& mutex)
    : mutex_(mutex) {
  mutex_.Acquire();
}

ScopedRecursiveLock::~ScopedRecursiveLock() {
  mutex_.Release();
}

}  // namespace starboard.

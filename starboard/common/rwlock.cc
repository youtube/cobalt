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

#include "starboard/common/log.h"

namespace starboard {

// Write-preferring lock.
// https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock
RWLock::RWLock()
    : readers_(0),
      writing_(false) {
  SB_CHECK(SbMutexCreate(&mutex_));
  SB_CHECK(SbConditionVariableCreate(&condition_, &mutex_));
}

RWLock::~RWLock() {
  SbConditionVariableDestroy(&condition_);
  SbMutexDestroy(&mutex_);
}

void RWLock::AcquireReadLock() {
  SB_CHECK(SbMutexAcquire(&mutex_) == kSbMutexAcquired);
  while (writing_) {
    SB_CHECK(SbConditionVariableWait(&condition_, &mutex_) !=
             kSbConditionVariableFailed);
  }
  ++readers_;
  SB_CHECK(SbMutexRelease(&mutex_));
}

void RWLock::ReleaseReadLock() {
  SB_CHECK(SbMutexAcquire(&mutex_) == kSbMutexAcquired);
  if (--readers_ == 0) {
    SB_CHECK(SbConditionVariableBroadcast(&condition_));
  }
  SB_CHECK(SbMutexRelease(&mutex_));
}

void RWLock::AcquireWriteLock() {
  SB_CHECK(SbMutexAcquire(&mutex_) == kSbMutexAcquired);
  while (writing_) {
    SB_CHECK(SbConditionVariableWait(&condition_, &mutex_) !=
             kSbConditionVariableFailed);
  }
  writing_ = true;
  while (readers_ > 0) {
    SB_CHECK(SbConditionVariableWait(&condition_, &mutex_) !=
             kSbConditionVariableFailed);
  }
  SB_CHECK(SbMutexRelease(&mutex_));
}

void RWLock::ReleaseWriteLock() {
  SB_CHECK(SbMutexAcquire(&mutex_) == kSbMutexAcquired);
  writing_ = false;
  SB_CHECK(SbConditionVariableBroadcast(&condition_));
  SB_CHECK(SbMutexRelease(&mutex_));
}

}  // namespace starboard

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

#ifndef STARBOARD_COMMON_RECURSIVE_MUTEX_H_
#define STARBOARD_COMMON_RECURSIVE_MUTEX_H_

#include "starboard/common/mutex.h"
#include "starboard/configuration.h"
#include "starboard/thread.h"

namespace starboard {

// A cross platform RecursiveMutex implementation. It is intended to replace
// std::recursive_mutex.
class RecursiveMutex {
 public:
  RecursiveMutex();
  ~RecursiveMutex();

  // The caller is blocked only if the mutex is being held by another thread.
  // If the caller already have the RecursiveMutex, Release() will succeed.
  // Otherwise Acquire() will block the caller until mutex is released.
  void Acquire();

  // The caller releases the mutex only when it calls Release() as many times
  // as Release().
  void Release();

  // Non-blocking function.
  // Acquires the RecursiveMutex if the mutex is free and returns true.
  // Only returns false if mutex is being held by another thread.
  bool AcquireTry();

 private:
  Mutex mutex_;
  SbThreadId owner_id_;
  // Only the owner is able to modify recurse_count_.
  size_t recurse_count_;

  RecursiveMutex(const RecursiveMutex&) = delete;
  void operator=(const RecursiveMutex&) = delete;
};

class ScopedRecursiveLock {
 public:
  explicit ScopedRecursiveLock(RecursiveMutex& mutex);
  ~ScopedRecursiveLock();

 private:
  RecursiveMutex& mutex_;

  ScopedRecursiveLock(const ScopedRecursiveLock&) = delete;
  void operator=(const ScopedRecursiveLock&) = delete;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_RECURSIVE_MUTEX_H_

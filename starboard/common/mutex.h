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

// Defines convenience classes that wrap the core Starboard mutually exclusive
// locking API to provide additional locking mechanisms.

#ifndef STARBOARD_COMMON_MUTEX_H_
#define STARBOARD_COMMON_MUTEX_H_

#include <pthread.h>

#include "starboard/configuration.h"

// TODO: b/390503926 - Remove the starboard mutex API and all it's
// references. See work done in 25lts as an example.

#ifdef __cplusplus

extern "C++" {

namespace starboard {

// Inline class wrapper for mutex.
class Mutex {
 public:
  Mutex();
  ~Mutex();

  void Acquire() const;
  bool AcquireTry() const;
  void Release() const;

 private:
  friend class ConditionVariable;

  pthread_mutex_t* mutex() const;
  mutable pthread_mutex_t mutex_;
  Mutex(const Mutex&) = delete;
  void operator=(const Mutex&) = delete;
};

// Scoped lock holder that works on starboard::Mutex.
class ScopedLock {
 public:
  explicit ScopedLock(const Mutex& mutex);
  ~ScopedLock();

 private:
  const Mutex& mutex_;

  ScopedLock(const ScopedLock&) = delete;
  void operator=(const ScopedLock&) = delete;
};

// Scoped lock holder that works on starboard::Mutex which uses AcquireTry()
// instead of Acquire().
class ScopedTryLock {
 public:
  explicit ScopedTryLock(const Mutex& mutex);
  ~ScopedTryLock();

  bool is_locked() const;

 private:
  const Mutex& mutex_;
  bool is_locked_;

  ScopedTryLock(const ScopedTryLock&) = delete;
  void operator=(const ScopedTryLock&) = delete;
};

}  // namespace starboard
}

#endif  //__cplusplus

#endif  // STARBOARD_COMMON_MUTEX_H_

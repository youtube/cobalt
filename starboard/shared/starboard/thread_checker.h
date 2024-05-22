// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_THREAD_CHECKER_H_
#define STARBOARD_SHARED_STARBOARD_THREAD_CHECKER_H_

#include <pthread.h>

#include "starboard/atomic.h"

namespace starboard {
namespace shared {
namespace starboard {

#if defined(COBALT_BUILD_TYPE_GOLD)

class ThreadChecker {
 public:
  enum Type { kSetThreadIdOnCreation, kSetThreadIdOnFirstCheck };

  explicit ThreadChecker(Type type = kSetThreadIdOnCreation) {}

  void Detach() {}

  bool CalledOnValidThread() const { return true; }
};

#else  // defined(COBALT_BUILD_TYPE_GOLD)

class ThreadChecker {
 public:
  enum Type { kSetThreadIdOnCreation, kSetThreadIdOnFirstCheck };

  explicit ThreadChecker(Type type = kSetThreadIdOnCreation) {
    if (type == kSetThreadIdOnCreation)
      thread_id_ =
          reinterpret_cast<uintptr_t>(reinterpret_cast<void*>(pthread_self()));
    else
      thread_id_ = 0;
  }

  // Detached the thread checker from its current thread.  The thread checker
  // will re-attach to a thread when CalledOnValidThread() is called again.
  // This essentially reset the thread checker to a status just like that it
  // was created with 'kSetThreadIdOnFirstCheck'.
  void Detach() {
    // This is safe as when this function is called, it is expected that it
    // won't be called on its current thread.
    thread_id_ = 0;
  }

  bool CalledOnValidThread() const {
    uintptr_t current_thread_id =
        reinterpret_cast<uintptr_t>(reinterpret_cast<void*>(pthread_self()));
#if SB_HAS(64_BIT_ATOMICS)
    uintptr_t stored_thread_id = SbAtomicNoBarrier_CompareAndSwap64(
        reinterpret_cast<SbAtomic64*>(&thread_id_), 0, current_thread_id);
#else
    uintptr_t stored_thread_id = SbAtomicNoBarrier_CompareAndSwap(
        reinterpret_cast<SbAtomic32*>(&thread_id_), 0, current_thread_id);
#endif

    return stored_thread_id == 0 || stored_thread_id == current_thread_id;
  }

 private:
  mutable uintptr_t thread_id_;
};

#endif  // defined(COBALT_BUILD_TYPE_GOLD)

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_THREAD_CHECKER_H_

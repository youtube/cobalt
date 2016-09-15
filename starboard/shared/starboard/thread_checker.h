// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/atomic.h"
#include "starboard/thread.h"

namespace starboard {
namespace shared {
namespace starboard {

#if defined(COBALT_BUILD_TYPE_GOLD)

class ThreadChecker {
 public:
  enum Type { kSetThreadIdOnCreation, kSetThreadIdOnFirstCheck };

  explicit ThreadChecker(Type type = kSetThreadIdOnCreation) {
    SB_UNREFERENCED_PARAMETER(type);
  }

  bool CalledOnValidThread() const { return true; }
};

#else  // defined(COBALT_BUILD_TYPE_GOLD)

class ThreadChecker {
 public:
  enum Type { kSetThreadIdOnCreation, kSetThreadIdOnFirstCheck };

  explicit ThreadChecker(Type type = kSetThreadIdOnCreation) {
    if (type == kSetThreadIdOnCreation)
      thread_id_ = SbThreadGetId();
    else
      thread_id_ = kSbThreadInvalidId;
  }

  bool CalledOnValidThread() const {
    SbThreadId current_thread_id = SbThreadGetId();
    SbThreadId stored_thread_id = SbAtomicNoBarrier_CompareAndSwap(
        &thread_id_, kSbThreadInvalidId, current_thread_id);
    return stored_thread_id == kSbThreadInvalidId ||
           stored_thread_id == current_thread_id;
  }

 private:
  mutable SbThreadId thread_id_;
};

#endif  // defined(COBALT_BUILD_TYPE_GOLD)

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_THREAD_CHECKER_H_

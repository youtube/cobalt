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

#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace starboard {

// A class that verifies that its methods are called on the same thread.
class ThreadChecker {
 public:
  ThreadChecker() : thread_id_(GetThreadId()) {
    SB_CHECK(SbThreadIsValidId(thread_id_));
  }

  bool CalledOnValidThread() const { return thread_id_ == GetThreadId(); }

 private:
  static inline SbThreadId GetThreadId() {
    // NOTE: We can cache the thread ID in thread-local storage since Cobalt
    // doesn't use fork(). If fork() were used, the child process would inherit
    // the stale cached ID, (which would then be stale), and we would need to
    // clear the TLS cache as is done in:
    // https://github.com/youtube/cobalt/blob/c38073920388e75c8a4451811e723562cf63ca58/base/threading/platform_thread_posix.cc
    thread_local SbThreadId tls_thread_id = kSbThreadInvalidId;
    if (tls_thread_id == kSbThreadInvalidId) {
      tls_thread_id = SbThreadGetId();
    }
    return tls_thread_id;
  }

  const SbThreadId thread_id_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_THREAD_CHECKER_H_

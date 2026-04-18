// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/thread_checker.h"

#if !defined(_WIN32)
#include <pthread.h>
#endif
#include <unistd.h>

#include <atomic>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace starboard {
namespace {

constexpr bool kEnableCacheTid = true;

thread_local SbThreadId tls_thread_id = kSbThreadInvalidId;

#if !defined(_WIN32)
void ChildHandler() {
  SB_CHECK(false) << "Fork detected! You should not use fork when Thread ID "
                     "caching is enabled.";
}

// Register the fork handler exactly once at startup.
bool g_fork_handler_registered = [] {
  if (kEnableCacheTid) {
    SB_CHECK_EQ(pthread_atfork(/*prepare=*/nullptr, /*parent=*/nullptr,
                               /*child=*/&ChildHandler),
                0);
  }
  return true;
}();
#endif  // !defined(_WIN32)

SbThreadId GetThreadId() {
  if (!kEnableCacheTid) {
    return SbThreadGetId();
  }

  if (tls_thread_id == kSbThreadInvalidId) {
    tls_thread_id = SbThreadGetId();
  }
  return tls_thread_id;
}
}  // namespace

ThreadChecker::ThreadChecker() : thread_id_(GetThreadId()) {
  SB_CHECK(SbThreadIsValidId(thread_id_));
}

bool ThreadChecker::CalledOnValidThread() const {
  return thread_id_ == GetThreadId();
}

}  // namespace starboard

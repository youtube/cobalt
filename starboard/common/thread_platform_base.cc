/*
 * Copyright 2025 The Cobalt Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// clang-format off
#include "starboard/common/thread_platform.h"
// clang-format on

#include <pthread.h>
#include <sys/resource.h>

#include "starboard/common/thread.h"

namespace starboard {

void SetCurrentThreadName(const char* name) {
  pthread_setname_np(pthread_self(), name);
}

bool SetCurrentThreadPriority(ThreadPriority priority) {
  // setpriority returns 0 on success and -1 on failure. The default nice value
  // is 0. See https://linux.die.net/man/2/setpriority
  return setpriority(PRIO_PROCESS, /*who=*/0,
                     ThreadPriorityToNiceValue(priority)) == 0;
}

void TerminateOnThread() {}

}  // namespace starboard

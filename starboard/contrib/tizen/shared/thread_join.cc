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

#include "starboard/thread.h"

#include <Eina.h>

#include "starboard/common/log.h"

#define THREAD_LOG_ON 0
#define USE_EINA_JOIN 0

bool SbThreadJoin(SbThread thread, void** out_return) {
  if (!SbThreadIsValid(thread)) {
    return false;
  }

#if THREAD_LOG_ON
  SB_DLOG(INFO) << "SbThreadJoin start ============= " << thread;
#endif
#if USE_EINA_JOIN
  // logically, we have to use eina's thread because we used eian's create.
  void* joined_return = eina_thread_join(thread);
#else
  // but it hides error so we may copy the code and work-around the problem.
  // refer /COMMON/Profile/platform/upstream/efl/src/lib/eina/eina_thread.c
  void* joined_return = NULL;
  int err = pthread_join((pthread_t)thread, &joined_return);
  if (err)
    return false;
#endif
#if THREAD_LOG_ON
  SB_DLOG(INFO) << "SbThreadJoin end ============= " << thread;
#endif

  if (out_return) {
    *out_return = joined_return;
  }

  return true;
}

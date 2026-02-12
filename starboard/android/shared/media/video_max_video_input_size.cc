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

#include "starboard/android/shared/media/video_max_video_input_size.h"

#include <pthread.h>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace starboard {

pthread_once_t s_once_flag = PTHREAD_ONCE_INIT;
pthread_key_t s_thread_local_key = 0;

void InitThreadLocalKey() {
  [[maybe_unused]] int res = pthread_key_create(&s_thread_local_key, NULL);
  SB_DCHECK_EQ(res, 0);
}

void EnsureThreadLocalKeyInited() {
  pthread_once(&s_once_flag, InitThreadLocalKey);
}

int GetMaxVideoInputSizeForCurrentThread() {
  EnsureThreadLocalKeyInited();
  // If the key is not valid or there is no value associated
  // with the key, it returns 0.
  return reinterpret_cast<uintptr_t>(pthread_getspecific(s_thread_local_key));
}

void SetMaxVideoInputSizeForCurrentThread(int max_video_input_size) {
  EnsureThreadLocalKeyInited();
  pthread_setspecific(s_thread_local_key,
                      reinterpret_cast<void*>(max_video_input_size));
}

}  // namespace starboard

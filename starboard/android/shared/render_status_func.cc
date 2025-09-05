// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/render_status_func.h"

#include <pthread.h>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace starboard::android::shared {

pthread_once_t s_render_status_cb_once_flag = PTHREAD_ONCE_INIT;
pthread_key_t s_render_status_cb_thread_local_key = 0;

void InitRenderStatusCBThreadLocalKey() {
  [[maybe_unused]] int res =
      pthread_key_create(&s_render_status_cb_thread_local_key, NULL);
  SB_DCHECK_EQ(res, 0);
}

void EnsureRenderStatusCBThreadLocalKeyInited() {
  pthread_once(&s_render_status_cb_once_flag, InitRenderStatusCBThreadLocalKey);
}

SbPlayerRenderStatusFunc GetRenderStatusCBForCurrentThread() {
  EnsureRenderStatusCBThreadLocalKeyInited();
  return reinterpret_cast<SbPlayerRenderStatusFunc>(
      pthread_getspecific(s_render_status_cb_thread_local_key));
}

void SetRenderStatusCBForCurrentThread(
    SbPlayerRenderStatusFunc render_status_func) {
  EnsureRenderStatusCBThreadLocalKeyInited();
  pthread_setspecific(s_render_status_cb_thread_local_key,
                      reinterpret_cast<void*>(render_status_func));
}

}  // namespace starboard::android::shared

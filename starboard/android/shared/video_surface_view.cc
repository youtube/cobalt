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

#include "starboard/android/shared/video_surface_view.h"

#include <pthread.h>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/thread.h"

namespace starboard::android::shared {

namespace {

pthread_once_t s_once_flag_for_video_surface_view = PTHREAD_ONCE_INIT;
pthread_key_t s_thread_local_key_for_video_surface_view = 0;

}  // namespace

void InitThreadLocalKeyForVideoSurfaceView() {
  [[maybe_unused]] int res =
      pthread_key_create(&s_thread_local_key_for_video_surface_view, nullptr);
  SB_DCHECK_EQ(res, 0);
}

void EnsureThreadLocalKeyInitedForVideoSurfaceView() {
  pthread_once(&s_once_flag_for_video_surface_view,
               InitThreadLocalKeyForVideoSurfaceView);
}

void* GetSurfaceViewForCurrentThread() {
  EnsureThreadLocalKeyInitedForVideoSurfaceView();
  // If the key is not valid or there is no value associated
  // with the key, it returns nullptr.
  return pthread_getspecific(s_thread_local_key_for_video_surface_view);
}

void SetVideoSurfaceViewForCurrentThread(void* surface_view) {
  EnsureThreadLocalKeyInitedForVideoSurfaceView();
  pthread_setspecific(s_thread_local_key_for_video_surface_view, surface_view);
}

}  // namespace starboard::android::shared

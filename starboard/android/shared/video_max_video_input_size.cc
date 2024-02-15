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

#include "starboard/android/shared/video_max_video_input_size.h"

#include "starboard/common/log.h"
#include "starboard/once.h"
#include "starboard/thread.h"

namespace starboard {
namespace android {
namespace shared {

SbOnceControl s_once_flag = SB_ONCE_INITIALIZER;
SbThreadLocalKey s_thread_local_key = kSbThreadLocalKeyInvalid;

void InitThreadLocalKey() {
  s_thread_local_key = SbThreadCreateLocalKey(NULL);
  SB_DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

void EnsureThreadLocalKeyInited() {
  SbOnce(&s_once_flag, InitThreadLocalKey);
  SB_DCHECK(SbThreadIsValidLocalKey(s_thread_local_key));
}

int GetMaxVideoInputSizeForCurrentThread() {
  EnsureThreadLocalKeyInited();
  // If the key is not valid or there is no value associated
  // with the key, it returns 0.
  return reinterpret_cast<uintptr_t>(SbThreadGetLocalValue(s_thread_local_key));
}

void SetMaxVideoInputSizeForCurrentThread(int max_video_input_size) {
  EnsureThreadLocalKeyInited();
  SbThreadSetLocalValue(s_thread_local_key,
                        reinterpret_cast<void*>(max_video_input_size));
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

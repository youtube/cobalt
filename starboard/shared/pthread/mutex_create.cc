// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/mutex.h"

#include <pthread.h>

#include "starboard/common/log.h"
#include "starboard/shared/pthread/is_success.h"
#include "starboard/shared/pthread/types_internal.h"
#include "starboard/shared/starboard/lazy_initialization_internal.h"

using starboard::shared::starboard::SetInitialized;

bool SbMutexCreate(SbMutex* mutex) {
#if SB_API_VERSION >= 12
  SB_COMPILE_ASSERT(sizeof(SbMutex) >= sizeof(SbMutexPrivate),
                    sb_mutex_private_larger_than_sb_mutex);
#endif
  if (!mutex) {
    return false;
  }

#if SB_API_VERSION >= 12
  SetInitialized(&(SB_INTERNAL_MUTEX(mutex)->initialized_state));
#endif
  return IsSuccess(pthread_mutex_init(SB_PTHREAD_INTERNAL_MUTEX(mutex), NULL));
}

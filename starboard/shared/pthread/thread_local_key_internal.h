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

#ifndef STARBOARD_SHARED_PTHREAD_THREAD_LOCAL_KEY_INTERNAL_H_
#define STARBOARD_SHARED_PTHREAD_THREAD_LOCAL_KEY_INTERNAL_H_

#if SB_API_VERSION < 16

#include <pthread.h>

#include "starboard/shared/internal_only.h"
#include "starboard/shared/pthread/is_success.h"
#include "starboard/thread.h"

struct SbThreadLocalKeyPrivate {
  // The underlying thread-local variable handle.
  pthread_key_t key;
};

#endif  // SB_API_VERSION < 16
#endif  // STARBOARD_SHARED_PTHREAD_THREAD_LOCAL_KEY_INTERNAL_H_

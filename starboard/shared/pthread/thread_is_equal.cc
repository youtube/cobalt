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

#include "starboard/thread.h"

#include <pthread.h>

#include "starboard/shared/pthread/types_internal.h"

bool SbThreadIsEqual(SbThread thread1, SbThread thread2) {
  return pthread_equal(SB_PTHREAD_INTERNAL_THREAD(thread1),
                       SB_PTHREAD_INTERNAL_THREAD(thread2)) != 0;
}

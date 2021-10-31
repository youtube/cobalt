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

#include "starboard/memory.h"

#include <pthread.h>
#include <pthread_np.h>

void SbMemoryGetStackBounds(void** out_high, void** out_low) {
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_get_np(pthread_self(), &attr);

  void* stack_address;
  size_t stack_size;
  pthread_attr_getstack(&attr, &stack_address, &stack_size);
  *out_high = static_cast<uint8_t*>(stack_address) + stack_size;
  *out_low = stack_address;

  pthread_attr_destroy(&attr);
}

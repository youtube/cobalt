// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software //
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "starboard/shared/modular/starboard_layer_posix_errno_abi_wrappers.h"

#include <pthread.h>
#include <stdlib.h>

static pthread_key_t g_errno_key = 0;
static pthread_once_t g_errno_once = PTHREAD_ONCE_INIT;

void initialize_errno_key() {
  pthread_key_create(&g_errno_key, free);
}

int* __abi_wrap___errno_location() {
  if (!errno_translation()) {
    return __errno_location();
  }
  int musl_errno = errno_to_musl_errno(errno);

  pthread_once(&g_errno_once, &initialize_errno_key);

  int* value = reinterpret_cast<int*>(pthread_getspecific(g_errno_key));

  if (value) {
    if (errno != 0) {
      *value = musl_errno;
      errno = 0;
    }

    return value;
  }

  value = reinterpret_cast<int*>(calloc(1, sizeof(int)));

  pthread_setspecific(g_errno_key, value);

  *value = musl_errno;
  errno = 0;

  return value;
}

// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef HB_STARBOARD_HH
#define HB_STARBOARD_HH

#include <string.h>

#include "starboard/memory.h"
#include "starboard/common/string.h"
#include "starboard/system.h"
#include "starboard/common/log.h"

<<<<<<<< HEAD:third_party/harfbuzz-ng/src/hb-starboard.hh
#define hb_malloc_impl malloc
#define hb_realloc_impl realloc
#define hb_calloc_impl calloc
#define hb_free_impl free
========
int __abi_wrap_ftruncate(int fildes, off_t length);

int ftruncate(int fildes, off_t length) {
  return __abi_wrap_ftruncate(fildes, length);
}

off_t __abi_wrap_lseek(int fildes, off_t offset, int whence);
>>>>>>>> 79adf53a067 (Add tests for POSIX file flush, truncate, and delete. (#3139)):starboard/shared/modular/cobalt_layer_posix_unistd_abi_wrappers.cc

// Micro Posix Emulation
#undef assert
#define assert SB_DCHECK
#define getenv(x) NULL

#endif  // HB_STARBOARD_HH

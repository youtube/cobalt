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

#define hb_malloc_impl SbMemoryAllocate
#define hb_realloc_impl SbMemoryReallocate
#define hb_calloc_impl SbMemoryCalloc
#define hb_free_impl SbMemoryDeallocate

// Micro Posix Emulation
#undef assert
#define assert SB_DCHECK
#define getenv(x) NULL
#define snprintf SbStringFormatF
#define strdup SbStringDuplicate

#endif  // HB_STARBOARD_HH

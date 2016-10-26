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

#ifndef WEBP_STARBOARD_PRIVATE_H_
#define WEBP_STARBOARD_PRIVATE_H_

#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/system.h"


static SB_C_INLINE int webp_abs(int a) {
  return (a < 0 ? -a : a);
}

#undef assert
#define assert(x) SB_DCHECK(x)

#define abs(x) webp_abs(x)
#define calloc SbMemoryCalloc
#define free SbMemoryDeallocate
#define malloc SbMemoryAllocate
#define memcpy SbMemoryCopy
#define memmove SbMemoryMove
#define memset SbMemorySet
#define qsort SbSystemSort


#endif  /* WEBP_STARBOARD_PRIVATE_H_ */

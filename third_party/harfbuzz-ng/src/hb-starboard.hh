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

#include "starboard/memory.h"
#include "starboard/string.h"
#include "starboard/system.h"
#include "starboard/log.h"

#define hb_malloc_impl SbMemoryAllocate
#define hb_realloc_impl SbMemoryReallocate
#define hb_calloc_impl SbMemoryCalloc
#define hb_free_impl SbMemoryDeallocate

// Micro Posix Emulation
#undef assert
#define assert SB_DCHECK
#define bsearch SbSystemBinarySearch
#define getenv(x) NULL
#define memcpy SbMemoryCopy
#define memmove SbMemoryMove
#define memset SbMemorySet
#define qsort SbSystemSort
#define snprintf SbStringFormatF
#define strchr (char *)SbStringFindCharacter
#define strcmp SbStringCompareAll
#define strcpy SbStringCopyUnsafe
#define strdup SbStringDuplicate
#define strlen SbStringGetLength
#define strncmp SbStringCompare
#define strncpy SbStringCopy
#define strstr SbStringFindString
#define strtol SbStringParseSignedInteger
#define strtoul SbStringParseUnsignedInteger

#endif  // HB_STARBOARD_HH

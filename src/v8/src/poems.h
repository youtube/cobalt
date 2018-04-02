// Copyright 2018 Google Inc. All Rights Reserved.
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

// Specialized poems for porting V8 on top of Starboard.  It would have been
// nice to have been able to use the shared poems, however they are too
// aggressive for V8 (such as grabbing identifiers that will come after std::).

#ifndef V8_POEMS_H_
#define V8_POEMS_H_

#if !defined(STARBOARD)
#error "Including V8 poems without STARBOARD defined."
#endif

#if !defined(V8_OS_STARBOARD)
#error "Including V8 poems without V8_OS_STARBOARD defined."
#endif

#include "starboard/memory.h"
#include "starboard/string.h"

#define malloc(x) SbMemoryAllocate(x)
#define realloc(x, y) SbMemoryReallocate(x, y)
#define free(x) SbMemoryDeallocate(x)
#define memcpy(x, y, z) SbMemoryCopy(x, y, z)
#define calloc(x, y) SbMemoryCalloc(x, y)
#define strdup(s) SbStringDuplicate(s)

#endif  // V8_POEMS_H_

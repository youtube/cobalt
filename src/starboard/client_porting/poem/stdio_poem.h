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

// A poem (POsix EMulation) for functions in stdio.h

#ifndef STARBOARD_CLIENT_PORTING_POEM_STDIO_POEM_H_
#define STARBOARD_CLIENT_PORTING_POEM_STDIO_POEM_H_

#if defined(STARBOARD)

#if !defined(POEM_NO_EMULATION)

#include "starboard/memory.h"
#include "starboard/string.h"

#define wcsncmp(s1, s2, c) SbStringCompareWide(s1, s2, c)

// the following functions can have variable number of arguments
// and, out of compatibility concerns, we chose to not use
// __VA_ARGS__ functionality.
#undef vsnprintf
#define vsnprintf SbStringFormat
#undef snprintf
#define snprintf SbStringFormatF
#undef sprintf
#define sprintf SbStringFormatUnsafeF
#undef vsscanf
#define vsscanf SbStringScan
#undef sscanf
#define sscanf SbStringScanF
#undef malloc
#define malloc(sz) SbMemoryAllocate(sz)
#undef calloc
#define calloc(c, s) SbMemoryCalloc(c, s)
#undef free
#define free(a) SbMemoryDeallocate(a)
#undef realloc
#define realloc(m, sz) SbMemoryReallocate(m, sz)

#endif  // POEM_NO_EMULATION

#endif  // STARBOARD

#endif  // STARBOARD_CLIENT_PORTING_POEM_STDIO_POEM_H_

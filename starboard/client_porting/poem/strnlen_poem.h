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

// A poem (POsix EMulation) implementation for strnlen. Usually declared in
// <string.h>, but may be missing on some platforms (e.g. PS3).

#ifndef STARBOARD_CLIENT_PORTING_POEM_STRNLEN_POEM_H_
#define STARBOARD_CLIENT_PORTING_POEM_STRNLEN_POEM_H_

#include "starboard/configuration.h"

static SB_C_INLINE size_t StringGetLengthFixed(const char* s, size_t maxlen) {
  size_t i = 0;
  while (i < maxlen && s[i]) {
    ++i;
  }
  return i;
}

#undef strnlen
#define strnlen(s, maxlen) StringGetLengthFixed(s, maxlen)

#endif  // STARBOARD_CLIENT_PORTING_POEM_STRNLEN_POEM_H_

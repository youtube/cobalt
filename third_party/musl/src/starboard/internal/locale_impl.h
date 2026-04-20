// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_MUSL_SRC_STARBOARD_INTERNAL_LOCALE_IMPL_H_
#define THIRD_PARTY_MUSL_SRC_STARBOARD_INTERNAL_LOCALE_IMPL_H_

#include <locale.h>
#include <stdlib.h>
#include "libc.h"

// Using a NULL value here is valid since this is ignored everywhere it is used.
#define CURRENT_LOCALE ((locale_t)0)

// Starboard supports UTF-16 and UTF-32, both of which have a maximum character
// size of 4 bytes.
#undef MB_CUR_MAX
#define MB_CUR_MAX 4

#endif  // THIRD_PARTY_MUSL_SRC_STARBOARD_INTERNAL_LOCALE_IMPL_H_

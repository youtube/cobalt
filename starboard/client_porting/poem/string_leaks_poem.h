// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

// This header plugs in the leaks of stdio functions called as std::<function>.

#ifndef STARBOARD_CLIENT_PORTING_POEM_STRING_LEAKS_POEM_H_
#define STARBOARD_CLIENT_PORTING_POEM_STRING_LEAKS_POEM_H_

// This header plugs in the leaks of string functions that are called as
// std::<function>. To plug these leaks we need to be able to strictly defined
// when <cstring> is included and redefine the desired functions afterwared.
// Additionally, unlike the other poems, we also need to define a function in
// the std namespace that forwards its call to the globally namespaced
// counterpart.
//
// *IMPORTANT*
// This header MUST be included at the top of the targeted file or we cannot
// guarantee when <cstring> will be included. The order of this include must be
// preserved or the leaks will not be plugged.

#include <cstring>

#include "starboard/common/string.h"

namespace std {

inline size_t SbStringGetLength(const char* str) {
  return ::SbStringGetLength(str);
}

}  // namespace std

// The following macros specify a fully qualified function call to
// SbStringGetLength to avoid function call ambiguities when building for
// Android.
//
#undef __builtin_strlen
#define __builtin_strlen std::SbStringGetLength
#undef strlen
#define strlen std::SbStringGetLength

#endif  // STARBOARD_CLIENT_PORTING_POEM_STRING_LEAKS_POEM_H_

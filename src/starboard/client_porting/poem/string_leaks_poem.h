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

// This header removes the Starboard API leaks of namespaced 'string' functions.

#ifndef STARBOARD_CLIENT_PORTING_POEM_STRING_LEAKS_POEM_H_
#define STARBOARD_CLIENT_PORTING_POEM_STRING_LEAKS_POEM_H_

// Starboard API leaks of 'string' functions can exist in the ::, ::std, and the
// ::std::INLINE_POEM_NAMESPACE namespaces. To remove these leaks we need to
// control when <cstring> is included and immediately redefine all of the
// desired functions.  Unlike traditional poems, we need to handle namespaces
// and avoid creating ambiguous function calls. The requirements are:
//
// 1) Use a different symbol name than the targeted symbol when creating the
//    functions to drop in, e.g. "SbStringGetLength" when
//    "SbStringGetLength" is targeted.
//
// 2) Define an inline function in the global namespace, and any other namespace
//    where the symbol needs to be replaced, that redirects directly to the
//    correct Starboard API.
//
// 3) Use macros to redefine the targeted symbol to the *globally namespaced*
//    replacement function. Do not include any namespacing in the macro.
//
// IMPORTANT: This header MUST be included at the top of any file with the
// appropriate leaks or the leaks will not be replaced.

#if defined(STARBOARD)

#include <cstring>

#include "starboard/common/string.h"

// We need three functions to appropriately remove Starboard API leaks for of
// our platforms. We need the ::, ::std, and ::std::INLINE_POEM_NAMESPACE
// namespaced symbols with a name different than the final, targeted symbol in
// order to remove the leaks across multiple namespaces while avoiding ambiguous
// function calls.

#if defined(INLINE_POEM_NAMESPACE)
inline size_t SbStringGetLengthRedirect(const char* str) {
  return ::SbStringGetLength(str);
}
namespace std {
inline size_t SbStringGetLengthRedirect(const char* str) {
  return ::SbStringGetLength(str);
}
inline namespace INLINE_POEM_NAMESPACE {
inline size_t SbStringGetLengthRedirect(const char* str) {
  return ::SbStringGetLength(str);
}
}  // namespace INLINE_POEM_NAMESPACE
}  // namespace std
#else   // defined(INLINE_POEM_NAMESPACE)
namespace std {
inline size_t SbStringGetLength(const char* str) {
  return ::SbStringGetLength(str);
}
}  // namespace std
#endif  // defined(INLINE_POEM_NAMESPACE)

#undef __builtin_strlen
#undef strlen

#if defined(INLINE_POEM_NAMESPACE)
#define __builtin_strlen SbStringGetLengthRedirect
#define strlen SbStringGetLengthRedirect
#else  // defined(INLINE_POEM_NAMESPACE)
#define __builtin_strlen std::SbStringGetLength
#define strlen std::SbStringGetLength
#endif  // defined(INLINE_POEM_NAMESPACE)

#endif  // STARBOARD

#endif  // STARBOARD_CLIENT_PORTING_POEM_STRING_LEAKS_POEM_H_

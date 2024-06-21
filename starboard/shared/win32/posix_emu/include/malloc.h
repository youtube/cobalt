// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_MALLOC_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_MALLOC_H_

#include <../ucrt/malloc.h>  // The Visual Studio version of this same file

// b/199773752, b/309016038: Remove when the PS5 SDK is updated past version 4.
#if __STDC_VERSION__ == 201112L && !defined(__cplusplus)
#define alignof _Alignof
#define static_assert _Static_assert
#endif

#ifdef __cplusplus
extern "C" {
#endif

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/free.html
// NOTE: Also declared in stdlib.h and implementation in stdlib.cc.
void sb_free(void* ptr);
#undef free
#define free sb_free

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_MALLOC_H_

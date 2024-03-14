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

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_STDLIB_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_STDLIB_H_

#include <../ucrt/stdlib.h>  // The Visual Studio version of this same file

#ifdef __cplusplus
extern "C" {
#endif

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/posix_memalign.html
int posix_memalign(void** res, size_t alignment, size_t size);

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/free.html
void sb_free(void* ptr);
#undef free
#define free sb_free

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_STDLIB_H_

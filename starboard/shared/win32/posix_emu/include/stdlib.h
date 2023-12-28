// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

// MSVC deprecated strdup() in favor of _strdup()
#define strdup _strdup
#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#if defined(STARBOARD)
#define free sb_free

#ifdef __cplusplus
extern "C" {
#endif

int posix_memalign(void** res, size_t alignment, size_t size);

#ifdef __cplusplus
}
#endif  // __cplusplus
#endif  // defined(STARBOARD)
#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_STDLIB_H_

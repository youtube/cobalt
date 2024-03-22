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
#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_STAT_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_STAT_H_

#include <../ucrt/sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

#define S_ISLNK(mode) 0  // Windows doesn't support symbolic links
#define S_ISDIR(mode) (((mode) & _S_IFMT) == (_S_IFDIR))

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_STAT_H_

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

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_MMAN_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_MMAN_H_

#include <sys/types.h>

#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4

#define MAP_PRIVATE 0x02
#define MAP_ANON 0x20

#define MAP_FAILED ((void*)-1)

#ifdef __cplusplus
extern "C" {
#endif

void* mmap(void*, size_t, int, int, int, off_t);
int munmap(void*, size_t);
int mprotect(void*, size_t, int);
int msync(void*, size_t, int);

#ifdef __cplusplus
}
#endif

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_SYS_MMAN_H_

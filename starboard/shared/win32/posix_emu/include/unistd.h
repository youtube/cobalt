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

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_UNISTD_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_UNISTD_H_

#include <io.h>
#include <starboard/types.h>

#undef open
#undef close

#ifdef __cplusplus
extern "C" {
#endif

int sb_close(int fd);
#define close sb_close

// The implementation of the following functions are located in socket.cc.

int fsync(int fd);

int ftruncate(int fd, int64_t length);

long lseek(int fd, long offset, int origin);  // NOLINT

int read(int fd, void* buffer, unsigned int buffer_size);

int write(int fd, const void* buffer, unsigned int count);

int usleep(unsigned int useconds);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_UNISTD_H_

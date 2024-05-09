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

#include <starboard/types.h>

#ifdef __cplusplus
extern "C" {
#endif

// The implementation of the following functions are located in socket.cc.

// This function will handle both files and sockets.
int close(int fd);

off_t sb_lseek(int fd, off_t offset, int origin);
#define lseek sb_lseek

ssize_t sb_read(int fildes, void* buf, size_t nbyte);
#define read sb_read

int usleep(unsigned int useconds);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_UNISTD_H_

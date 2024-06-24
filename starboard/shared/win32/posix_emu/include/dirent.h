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
#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_DIRENT_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_DIRENT_H_

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

struct __dirstream {
  int tell;
  int fd;
  int buf_pos;
  int buf_end;
  volatile int lock[1];
  HANDLE handle;
  char buf[2048];
};

struct dirent {
  int d_ino;
  int d_off;
  int d_reclen;
  unsigned char d_type;
  char d_name[256];
};

typedef struct __dirstream DIR;

DIR* opendir(const char* path);

int closedir(DIR* dir);

int readdir_r(DIR* __restrict dir,
              struct dirent* __restrict dirent_buf,
              struct dirent** __restrict dirent);

#ifdef __cplusplus
}
#endif  // __cplusplus

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_DIRENT_H_

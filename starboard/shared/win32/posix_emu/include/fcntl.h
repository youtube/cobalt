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

#ifndef STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_FCNTL_H_
#define STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_FCNTL_H_

#include <../ucrt/fcntl.h>  // The Visual Studio version of this same file
#include <io.h>             // Needed for `open`, which is in fcntl.h on POSIX
#include <sys/stat.h>

#undef open
#undef close  // in unistd.h on POSIX, and handles both files and sockets

typedef int mode_t;

// For the POSIX modes that do not have a Windows equivalent, the modes
// defined here use the POSIX values left shifted 16 bits.
// Passing these into Windows file I/O functions has no effect.

static const mode_t S_ISUID = 0x40000000;
static const mode_t S_ISGID = 0x20000000;
static const mode_t S_ISVTX = 0x10000000;
static const mode_t S_IRWXU = 0x07000000;
static const mode_t S_IRUSR = _S_IREAD;   // read by user
static const mode_t S_IWUSR = _S_IWRITE;  // write by user
static const mode_t S_IXUSR = 0x01000000;
static const mode_t S_IRGRP = 0x00400000;
static const mode_t S_IWGRP = 0x00200000;
static const mode_t S_IXGRP = 0x00100000;
static const mode_t S_IRWXO = 0x00070000;
static const mode_t S_IROTH = 0x00040000;
static const mode_t S_IWOTH = 0x00020000;
static const mode_t S_IXOTH = 0x00010000;

static const mode_t MS_MODE_MASK = 0x0000ffff;

int open(const char* path, int oflag, ...);

#endif  // STARBOARD_SHARED_WIN32_POSIX_EMU_INCLUDE_FCNTL_H_

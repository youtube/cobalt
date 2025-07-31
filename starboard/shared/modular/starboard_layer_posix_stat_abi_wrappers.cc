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

#include "starboard/shared/modular/starboard_layer_posix_stat_abi_wrappers.h"

#include <sys/stat.h>

static_assert(S_ISUID == 04000,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_ISGID == 02000,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_ISVTX == 01000,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_IRWXU == 0700,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_IRUSR == 0400,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_IWUSR == 0200,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_IXUSR == 0100,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_IRWXG == 070,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_IRGRP == 040,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_IWGRP == 020,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_IXGRP == 010,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_IRWXO == 07,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_IROTH == 04,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_IWOTH == 02,
              "The Starboard layer wrapper expects this value from musl");
static_assert(S_IXOTH == 01,
              "The Starboard layer wrapper expects this value from musl");

namespace {
int stat_helper(int retval,
                struct stat* stat_info,
                struct musl_stat* musl_info) {
  if (retval != 0 || musl_info == NULL) {
    return -1;
  }

  musl_info->st_size = stat_info->st_size;
  musl_info->st_mode = stat_info->st_mode;

  musl_info->st_atim.tv_sec = stat_info->st_atim.tv_sec;
  musl_info->st_atim.tv_nsec = stat_info->st_atim.tv_nsec;
  musl_info->st_mtim.tv_sec = stat_info->st_mtim.tv_sec;
  musl_info->st_mtim.tv_nsec = stat_info->st_mtim.tv_nsec;
  musl_info->st_ctim.tv_sec = stat_info->st_ctim.tv_sec;
  musl_info->st_ctim.tv_nsec = stat_info->st_ctim.tv_nsec;

  musl_info->st_nlink = stat_info->st_nlink;
  musl_info->st_uid = stat_info->st_uid;
  musl_info->st_gid = stat_info->st_gid;
  musl_info->st_ino = stat_info->st_ino;
  musl_info->st_dev = stat_info->st_dev;
  musl_info->st_rdev = stat_info->st_rdev;
  musl_info->st_blksize = stat_info->st_blksize;
  musl_info->st_blocks = stat_info->st_blocks;

  return retval;
}

mode_t musl_mode_to_platform_mode(musl_mode_t musl_mode) {
  mode_t platform_mode = 0;

  // File type
  if ((musl_mode & MUSL_S_IFMT) == MUSL_S_IFDIR) {
    platform_mode |= S_IFDIR;
  }
  if ((musl_mode & MUSL_S_IFMT) == MUSL_S_IFCHR) {
    platform_mode |= S_IFCHR;
  }
  if ((musl_mode & MUSL_S_IFMT) == MUSL_S_IFBLK) {
    platform_mode |= S_IFBLK;
  }
  if ((musl_mode & MUSL_S_IFMT) == MUSL_S_IFREG) {
    platform_mode |= S_IFREG;
  }
  if ((musl_mode & MUSL_S_IFMT) == MUSL_S_IFIFO) {
    platform_mode |= S_IFIFO;
  }
  if ((musl_mode & MUSL_S_IFMT) == MUSL_S_IFLNK) {
    platform_mode |= S_IFLNK;
  }
  if ((musl_mode & MUSL_S_IFMT) == MUSL_S_IFSOCK) {
    platform_mode |= S_IFSOCK;
  }

  // Permissions
  platform_mode |= (musl_mode & 07777);

  return platform_mode;
}
}  // namespace

int __abi_wrap_fstat(int fildes, struct musl_stat* musl_info) {
  struct stat stat_info;  // The type from platform toolchain.
  int retval = fstat(fildes, &stat_info);
  return stat_helper(retval, &stat_info, musl_info);
}

int __abi_wrap_lstat(const char* path, struct musl_stat* musl_info) {
  struct stat stat_info;  // The type from platform toolchain.
  int retval = lstat(path, &stat_info);
  return stat_helper(retval, &stat_info, musl_info);
}

int __abi_wrap_stat(const char* path, struct musl_stat* musl_info) {
  struct stat stat_info;  // The type from platform toolchain.
  int retval = stat(path, &stat_info);
  return stat_helper(retval, &stat_info, musl_info);
}

int __abi_wrap_chmod(const char* path, musl_mode_t mode) {
  return chmod(path, musl_mode_to_platform_mode(mode));
}

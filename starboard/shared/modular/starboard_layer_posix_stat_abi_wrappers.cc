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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
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

int stat_helper(int retval,
                struct stat* stat_info,
                struct musl_stat* musl_info) {
  if (retval != 0 || musl_info == NULL) {
    return -1;
  }

  musl_info->st_dev = stat_info->st_dev;
  musl_info->st_ino = stat_info->st_ino;
  musl_info->st_nlink = stat_info->st_nlink;
  musl_info->st_uid = stat_info->st_uid;
  musl_info->st_gid = stat_info->st_gid;
  musl_info->st_rdev = stat_info->st_rdev;
  musl_info->st_blksize = stat_info->st_blksize;
  musl_info->st_blocks = stat_info->st_blocks;
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

int musl_flag_to_platform_flag(int musl_flag) {
  switch (musl_flag) {
    case 0:
      return 0;
    case MUSL_AT_EMPTY_PATH:
      return AT_EMPTY_PATH;
    case MUSL_AT_SYMLINK_NOFOLLOW:
      return AT_SYMLINK_NOFOLLOW;
    default:
      errno = EINVAL;
      return -1;
  }
}

__MUSL_LONG_TYPE musl_nsec_to_platform_nsec(__MUSL_LONG_TYPE musl_nsec) {
  switch (musl_nsec) {
    case MUSL_UTIME_NOW:
      return UTIME_NOW;
    case MUSL_UTIME_OMIT:
      return UTIME_OMIT;
    default:
      return musl_nsec;
  }
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

int __abi_wrap_fchmod(int fd, musl_mode_t mode) {
  return fchmod(fd, musl_mode_to_platform_mode(mode));
}

int __abi_wrap_utimensat(int fildes,
                         const char* path,
                         const struct musl_timespec musl_times[2],
                         int musl_flag) {
  fildes = (fildes == MUSL_AT_FDCWD) ? AT_FDCWD : fildes;

  int flag = musl_flag_to_platform_flag(musl_flag);
  if (flag == -1) {
    return -1;
  }

  // If path is |NULL| or |nullptr|, set the path to the empty string and enable
  // the |AT_EMPTY_PATH| flag to ensure that utimensat does not try to use the
  // path. This can allow other functions who call utimensat without a file path
  // (futimes, futimesat) to still work as intended (some platforms do not allow
  // utimensat() to accept NULL as the value for |path|).
  if (!path) {
    path = "";
    flag |= AT_EMPTY_PATH;
  } else if ((strcmp(path, "") == 0) && ((flag & AT_EMPTY_PATH) == 0)) {
    // If utimensat is called with |path| set as the empty string but without
    // the |AT_EMPTY_PATH| flag enabled, we set errno to ENOENT and return -1.
    // |path| should not be set as the empty string without the |AT_EMPTY_PATH|
    // enabled.
    errno = ENOENT;
    return -1;
  }

  const struct timespec* platform_times = nullptr;
  struct timespec times[2];
  if (musl_times) {
    for (int i = 0; i < 2; i++) {
      times[i].tv_nsec = musl_nsec_to_platform_nsec(musl_times[i].tv_nsec);
      times[i].tv_sec = musl_times[i].tv_sec;
    }
    platform_times = times;
  }
  return utimensat(fildes, path, platform_times, flag);
}

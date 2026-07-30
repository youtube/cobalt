// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_statvfs_abi_wrappers.h"

int statvfs_helper(int retval,
                   const struct statvfs* statvfs_info,
                   struct musl_statvfs* musl_info) {
  if (retval != 0 || musl_info == NULL) {
    return -1;
  }

  musl_info->f_bsize = statvfs_info->f_bsize;
  musl_info->f_frsize = statvfs_info->f_frsize;
  musl_info->f_blocks = statvfs_info->f_blocks;
  musl_info->f_bfree = statvfs_info->f_bfree;
  musl_info->f_bavail = statvfs_info->f_bavail;
  musl_info->f_files = statvfs_info->f_files;
  musl_info->f_ffree = statvfs_info->f_ffree;
  musl_info->f_favail = statvfs_info->f_favail;
  musl_info->f_fsid = statvfs_info->f_fsid;
  musl_info->f_flag = statvfs_info->f_flag;
  musl_info->f_namemax = statvfs_info->f_namemax;

  return retval;
}

int __abi_wrap_statvfs(const char* path, struct musl_statvfs* buf) {
  struct statvfs statvfs_info;
  int retval = statvfs(path, &statvfs_info);
  return statvfs_helper(retval, &statvfs_info, buf);
}

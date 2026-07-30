// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/modular/starboard_layer_posix_inotify_abi_wrappers.h"

#include <errno.h>
#include <sys/inotify.h>

#include "starboard/common/log.h"

namespace {

int musl_flags_to_platform_flags(int musl_flags) {
  int platform_flags = 0;

  if (musl_flags & MUSL_IN_NONBLOCK) {
    platform_flags |= IN_NONBLOCK;
  }
  if (musl_flags & MUSL_IN_CLOEXEC) {
    platform_flags |= IN_CLOEXEC;
  }

  // Check for unknown flags.
  if (musl_flags & ~(MUSL_IN_NONBLOCK | MUSL_IN_CLOEXEC)) {
    return -1;
  }

  return platform_flags;
}

uint32_t musl_mask_to_platform_mask(uint32_t musl_mask) {
  uint32_t platform_mask = 0;

  if (musl_mask & MUSL_IN_ACCESS) {
    platform_mask |= IN_ACCESS;
  }
  if (musl_mask & MUSL_IN_MODIFY) {
    platform_mask |= IN_MODIFY;
  }
  if (musl_mask & MUSL_IN_ATTRIB) {
    platform_mask |= IN_ATTRIB;
  }
  if (musl_mask & MUSL_IN_CLOSE_WRITE) {
    platform_mask |= IN_CLOSE_WRITE;
  }
  if (musl_mask & MUSL_IN_CLOSE_NOWRITE) {
    platform_mask |= IN_CLOSE_NOWRITE;
  }
  if (musl_mask & MUSL_IN_OPEN) {
    platform_mask |= IN_OPEN;
  }
  if (musl_mask & MUSL_IN_MOVED_FROM) {
    platform_mask |= IN_MOVED_FROM;
  }
  if (musl_mask & MUSL_IN_MOVED_TO) {
    platform_mask |= IN_MOVED_TO;
  }
  if (musl_mask & MUSL_IN_CREATE) {
    platform_mask |= IN_CREATE;
  }
  if (musl_mask & MUSL_IN_DELETE) {
    platform_mask |= IN_DELETE;
  }
  if (musl_mask & MUSL_IN_DELETE_SELF) {
    platform_mask |= IN_DELETE_SELF;
  }
  if (musl_mask & MUSL_IN_MOVE_SELF) {
    platform_mask |= IN_MOVE_SELF;
  }
  if (musl_mask & MUSL_IN_UNMOUNT) {
    platform_mask |= IN_UNMOUNT;
  }
  if (musl_mask & MUSL_IN_Q_OVERFLOW) {
    platform_mask |= IN_Q_OVERFLOW;
  }
  if (musl_mask & MUSL_IN_IGNORED) {
    platform_mask |= IN_IGNORED;
  }
  if (musl_mask & MUSL_IN_ONLYDIR) {
    platform_mask |= IN_ONLYDIR;
  }
  if (musl_mask & MUSL_IN_DONT_FOLLOW) {
    platform_mask |= IN_DONT_FOLLOW;
  }
  if (musl_mask & MUSL_IN_EXCL_UNLINK) {
    platform_mask |= IN_EXCL_UNLINK;
  }
#ifdef IN_MASK_CREATE
  if (musl_mask & MUSL_IN_MASK_CREATE) {
    platform_mask |= IN_MASK_CREATE;
  }
#endif
  if (musl_mask & MUSL_IN_MASK_ADD) {
    platform_mask |= IN_MASK_ADD;
  }
  if (musl_mask & MUSL_IN_ISDIR) {
    platform_mask |= IN_ISDIR;
  }
  if (musl_mask & MUSL_IN_ONESHOT) {
    platform_mask |= IN_ONESHOT;
  }

  return platform_mask;
}

bool is_valid_musl_mask(uint32_t mask) {
  uint32_t all_masks =
      MUSL_IN_ACCESS | MUSL_IN_MODIFY | MUSL_IN_ATTRIB | MUSL_IN_CLOSE_WRITE |
      MUSL_IN_CLOSE_NOWRITE | MUSL_IN_OPEN | MUSL_IN_MOVED_FROM |
      MUSL_IN_MOVED_TO | MUSL_IN_CREATE | MUSL_IN_DELETE | MUSL_IN_DELETE_SELF |
      MUSL_IN_MOVE_SELF | MUSL_IN_UNMOUNT | MUSL_IN_Q_OVERFLOW |
      MUSL_IN_IGNORED | MUSL_IN_ONLYDIR | MUSL_IN_DONT_FOLLOW |
      MUSL_IN_EXCL_UNLINK | MUSL_IN_MASK_ADD | MUSL_IN_ISDIR | MUSL_IN_ONESHOT;

#ifdef IN_MASK_CREATE
  all_masks |= MUSL_IN_MASK_CREATE;
#endif

  return (mask & ~all_masks) == 0;
}

}  // namespace

extern "C" {

SB_EXPORT int __abi_wrap_inotify_init1(int flags) {
  int platform_flags = musl_flags_to_platform_flags(flags);
  if (platform_flags == -1) {
    errno = EINVAL;
    return -1;
  }

  return inotify_init1(platform_flags);
}

SB_EXPORT int __abi_wrap_inotify_add_watch(int fd,
                                           const char* pathname,
                                           uint32_t mask) {
  if (!is_valid_musl_mask(mask)) {
    SB_LOG(WARNING) << "Invalid musl inotify mask: " << mask;
    errno = EINVAL;
    return -1;
  }
  return inotify_add_watch(fd, pathname, musl_mask_to_platform_mask(mask));
}

SB_EXPORT int __abi_wrap_inotify_rm_watch(int fd, int wd) {
  return inotify_rm_watch(fd, wd);
}

}  // extern "C"

// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

// Adapted from base/platform_file_posix.cc

#ifndef STARBOARD_SHARED_POSIX_IMPL_FILE_OPEN_H_
#define STARBOARD_SHARED_POSIX_IMPL_FILE_OPEN_H_

#include "starboard/file.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "starboard/common/log.h"
#include "starboard/shared/posix/handle_eintr.h"

#include "starboard/shared/internal_only.h"
#include "starboard/shared/posix/impl/file_impl.h"

namespace starboard {
namespace shared {
namespace posix {
namespace impl {

SbFile FileOpen(const char* path,
                int flags,
                bool* out_created,
                SbFileError* out_error) {
  int open_flags = 0;
  if (flags & kSbFileCreateOnly) {
    open_flags = O_CREAT | O_EXCL;
  }

  if (out_created) {
    *out_created = false;
  }

  if (flags & kSbFileCreateAlways) {
    SB_DCHECK(!open_flags);
    open_flags = O_CREAT | O_TRUNC;
  }

  if (flags & kSbFileOpenTruncated) {
    SB_DCHECK(!open_flags);
    SB_DCHECK(flags & kSbFileWrite);
    open_flags = O_TRUNC;
  }

  if (!open_flags && !(flags & kSbFileOpenOnly) &&
      !(flags & kSbFileOpenAlways)) {
    SB_NOTREACHED();
    errno = EOPNOTSUPP;
    if (out_error) {
      *out_error = kSbFileErrorFailed;
    }

    return kSbFileInvalid;
  }

  if (flags & kSbFileWrite && flags & kSbFileRead) {
    open_flags |= O_RDWR;
  } else if (flags & kSbFileWrite) {
    open_flags |= O_WRONLY;
  }

  SB_COMPILE_ASSERT(O_RDONLY == 0, O_RDONLY_must_equal_zero);

  int mode = S_IRUSR | S_IWUSR;
  int descriptor = HANDLE_EINTR(open(path, open_flags, mode));

  if (flags & kSbFileOpenAlways) {
    if (descriptor < 0) {
      open_flags |= O_CREAT;
      descriptor = HANDLE_EINTR(open(path, open_flags, mode));
      if (out_created && descriptor >= 0) {
        *out_created = true;
      }
    }
  }

  if (out_created && (descriptor >= 0) &&
      (flags & (kSbFileCreateAlways | kSbFileCreateOnly))) {
    *out_created = true;
  }

  if (out_error) {
    if (descriptor >= 0) {
      *out_error = kSbFileOk;
    } else {
      switch (errno) {
        case EACCES:
        case EISDIR:
        case EROFS:
        case EPERM:
          *out_error = kSbFileErrorAccessDenied;
          break;
        case ETXTBSY:
          *out_error = kSbFileErrorInUse;
          break;
        case EEXIST:
          *out_error = kSbFileErrorExists;
          break;
        case ENOENT:
          *out_error = kSbFileErrorNotFound;
          break;
        case EMFILE:
          *out_error = kSbFileErrorTooManyOpened;
          break;
        case ENOMEM:
          *out_error = kSbFileErrorNoMemory;
          break;
        case ENOSPC:
          *out_error = kSbFileErrorNoSpace;
          break;
        case ENOTDIR:
          *out_error = kSbFileErrorNotADirectory;
          break;
        case EIO:
          *out_error = kSbFileErrorIO;
          break;
        default:
          *out_error = kSbFileErrorFailed;
      }
    }
  }

  if (descriptor < 0) {
    return kSbFileInvalid;
  }

  SbFile result = new SbFilePrivate();
  result->descriptor = descriptor;
  return result;
}

}  // namespace impl
}  // namespace posix
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_POSIX_IMPL_FILE_OPEN_H_

// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_file.h"

#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"

namespace base {

#if defined(OS_OPENBSD) || defined(OS_FREEBSD) || \
    (defined(OS_MACOSX) && \
     MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_5)
typedef struct stat stat_wrapper_t;
static int CallFstat(int fd, stat_wrapper_t *sb) {
  return fstat(fd, sb);
}
#else
typedef struct stat64 stat_wrapper_t;
static int CallFstat(int fd, stat_wrapper_t *sb) {
  return fstat64(fd, sb);
}
#endif

// TODO(erikkay): does it make sense to support PLATFORM_FILE_EXCLUSIVE_* here?
PlatformFile CreatePlatformFile(const FilePath& name, int flags,
                                bool* created, PlatformFileError* error_code) {
  int open_flags = 0;
  if (flags & PLATFORM_FILE_CREATE)
    open_flags = O_CREAT | O_EXCL;

  if (flags & PLATFORM_FILE_CREATE_ALWAYS) {
    DCHECK(!open_flags);
    open_flags = O_CREAT | O_TRUNC;
  }

  if (!open_flags && !(flags & PLATFORM_FILE_OPEN) &&
      !(flags & PLATFORM_FILE_OPEN_ALWAYS)) {
    NOTREACHED();
    errno = EOPNOTSUPP;
    if (error_code)
      *error_code = PLATFORM_FILE_ERROR_FAILED;
    return kInvalidPlatformFileValue;
  }

  if (flags & PLATFORM_FILE_WRITE && flags & PLATFORM_FILE_READ) {
    open_flags |= O_RDWR;
  } else if (flags & PLATFORM_FILE_WRITE) {
    open_flags |= O_WRONLY;
  } else if (!(flags & PLATFORM_FILE_READ) &&
             !(flags & PLATFORM_FILE_WRITE_ATTRIBUTES) &&
             !(flags & PLATFORM_FILE_OPEN_ALWAYS)) {
    NOTREACHED();
  }

  if (flags & PLATFORM_FILE_TRUNCATE) {
    DCHECK(flags & PLATFORM_FILE_WRITE);
    open_flags |= O_TRUNC;
  }

  COMPILE_ASSERT(O_RDONLY == 0, O_RDONLY_must_equal_zero);

  int descriptor = open(name.value().c_str(), open_flags, S_IRUSR | S_IWUSR);

  if (flags & PLATFORM_FILE_OPEN_ALWAYS) {
    if (descriptor > 0) {
      if (created)
        *created = false;
    } else {
      open_flags |= O_CREAT;
      if (flags & PLATFORM_FILE_EXCLUSIVE_READ ||
          flags & PLATFORM_FILE_EXCLUSIVE_WRITE) {
        open_flags |= O_EXCL;   // together with O_CREAT implies O_NOFOLLOW
      }
      descriptor = open(name.value().c_str(), open_flags, S_IRUSR | S_IWUSR);
      if (created && descriptor > 0)
        *created = true;
    }
  }

  if (created && (descriptor > 0) &&
      (flags & (PLATFORM_FILE_CREATE_ALWAYS | PLATFORM_FILE_CREATE)))
    *created = true;

  if ((descriptor > 0) && (flags & PLATFORM_FILE_DELETE_ON_CLOSE)) {
    unlink(name.value().c_str());
  }

  if (error_code) {
    if (descriptor >= 0)
      *error_code = PLATFORM_FILE_OK;
    else {
      switch (errno) {
        case EACCES:
        case EISDIR:
        case EROFS:
        case EPERM:
          *error_code = PLATFORM_FILE_ERROR_ACCESS_DENIED;
          break;
        case ETXTBSY:
          *error_code = PLATFORM_FILE_ERROR_IN_USE;
          break;
        case EEXIST:
          *error_code = PLATFORM_FILE_ERROR_EXISTS;
          break;
        case ENOENT:
          *error_code = PLATFORM_FILE_ERROR_NOT_FOUND;
          break;
        case EMFILE:
          *error_code = PLATFORM_FILE_ERROR_TOO_MANY_OPENED;
          break;
        case ENOMEM:
          *error_code = PLATFORM_FILE_ERROR_NO_MEMORY;
          break;
        case ENOSPC:
          *error_code = PLATFORM_FILE_ERROR_NO_SPACE;
          break;
        case ENOTDIR:
          *error_code = PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
          break;
        default:
          *error_code = PLATFORM_FILE_ERROR_FAILED;
      }
    }
  }

  return descriptor;
}

bool ClosePlatformFile(PlatformFile file) {
  return !close(file);
}

int ReadPlatformFile(PlatformFile file, int64 offset, char* data, int size) {
  if (file < 0)
    return -1;

  return HANDLE_EINTR(pread(file, data, size, offset));
}

int WritePlatformFile(PlatformFile file, int64 offset,
                      const char* data, int size) {
  if (file < 0)
    return -1;

  return HANDLE_EINTR(pwrite(file, data, size, offset));
}

bool TruncatePlatformFile(PlatformFile file, int64 length) {
  return ((file >= 0) && !HANDLE_EINTR(ftruncate(file, length)));
}

bool FlushPlatformFile(PlatformFile file) {
  return !fsync(file);
}

bool TouchPlatformFile(PlatformFile file, const base::Time& last_access_time,
                       const base::Time& last_modified_time) {
  if (file < 0)
    return false;

  timeval times[2];
  times[0] = last_access_time.ToTimeVal();
  times[1] = last_modified_time.ToTimeVal();
  return !futimes(file, times);
}

bool GetPlatformFileInfo(PlatformFile file, PlatformFileInfo* info) {
  if (!info)
    return false;

  stat_wrapper_t file_info;
  if (CallFstat(file, &file_info))
    return false;

  info->is_directory = S_ISDIR(file_info.st_mode);
  info->is_symbolic_link = S_ISLNK(file_info.st_mode);
  info->size = file_info.st_size;
  info->last_modified = base::Time::FromTimeT(file_info.st_mtime);
  info->last_accessed = base::Time::FromTimeT(file_info.st_atime);
  info->creation_time = base::Time::FromTimeT(file_info.st_ctime);
  return true;
}

}  // namespace base

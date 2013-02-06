// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/platform_file.h"

#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"

#if defined(OS_ANDROID)
#include "base/os_compat_android.h"
#endif

namespace base {

// Make sure our Whence mappings match the system headers.
COMPILE_ASSERT(PLATFORM_FILE_FROM_BEGIN   == SEEK_SET &&
               PLATFORM_FILE_FROM_CURRENT == SEEK_CUR &&
               PLATFORM_FILE_FROM_END     == SEEK_END, whence_matches_system);

#if defined(OS_BSD) || defined(OS_MACOSX)
typedef struct stat stat_wrapper_t;
static int CallFstat(int fd, stat_wrapper_t *sb) {
  base::ThreadRestrictions::AssertIOAllowed();
  return fstat(fd, sb);
}
#else
typedef struct stat64 stat_wrapper_t;
static int CallFstat(int fd, stat_wrapper_t *sb) {
  base::ThreadRestrictions::AssertIOAllowed();
  return fstat64(fd, sb);
}
#endif

// TODO(erikkay): does it make sense to support PLATFORM_FILE_EXCLUSIVE_* here?
PlatformFile CreatePlatformFileUnsafe(const FilePath& name,
                                      int flags,
                                      bool* created,
                                      PlatformFileError* error) {
  base::ThreadRestrictions::AssertIOAllowed();

  int open_flags = 0;
  if (flags & PLATFORM_FILE_CREATE)
    open_flags = O_CREAT | O_EXCL;

  if (created)
    *created = false;

  if (flags & PLATFORM_FILE_CREATE_ALWAYS) {
    DCHECK(!open_flags);
    open_flags = O_CREAT | O_TRUNC;
  }

  if (flags & PLATFORM_FILE_OPEN_TRUNCATED) {
    DCHECK(!open_flags);
    DCHECK(flags & PLATFORM_FILE_WRITE);
    open_flags = O_TRUNC;
  }

  if (!open_flags && !(flags & PLATFORM_FILE_OPEN) &&
      !(flags & PLATFORM_FILE_OPEN_ALWAYS)) {
    NOTREACHED();
    errno = EOPNOTSUPP;
    if (error)
      *error = PLATFORM_FILE_ERROR_FAILED;
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

  if (flags & PLATFORM_FILE_TERMINAL_DEVICE)
    open_flags |= O_NOCTTY | O_NDELAY;

  COMPILE_ASSERT(O_RDONLY == 0, O_RDONLY_must_equal_zero);

  int mode = S_IRUSR | S_IWUSR;
#if defined(OS_CHROMEOS)
  mode |= S_IRGRP | S_IROTH;
#endif

  int descriptor =
      HANDLE_EINTR(open(name.value().c_str(), open_flags, mode));

  if (flags & PLATFORM_FILE_OPEN_ALWAYS) {
    if (descriptor < 0) {
      open_flags |= O_CREAT;
      if (flags & PLATFORM_FILE_EXCLUSIVE_READ ||
          flags & PLATFORM_FILE_EXCLUSIVE_WRITE) {
        open_flags |= O_EXCL;   // together with O_CREAT implies O_NOFOLLOW
      }
      descriptor = HANDLE_EINTR(
          open(name.value().c_str(), open_flags, mode));
      if (created && descriptor >= 0)
        *created = true;
    }
  }

  if (created && (descriptor >= 0) &&
      (flags & (PLATFORM_FILE_CREATE_ALWAYS | PLATFORM_FILE_CREATE)))
    *created = true;

  if ((descriptor >= 0) && (flags & PLATFORM_FILE_DELETE_ON_CLOSE)) {
    unlink(name.value().c_str());
  }

  if (error) {
    if (descriptor >= 0)
      *error = PLATFORM_FILE_OK;
    else {
      switch (errno) {
        case EACCES:
        case EISDIR:
        case EROFS:
        case EPERM:
          *error = PLATFORM_FILE_ERROR_ACCESS_DENIED;
          break;
        case ETXTBSY:
          *error = PLATFORM_FILE_ERROR_IN_USE;
          break;
        case EEXIST:
          *error = PLATFORM_FILE_ERROR_EXISTS;
          break;
        case ENOENT:
          *error = PLATFORM_FILE_ERROR_NOT_FOUND;
          break;
        case EMFILE:
          *error = PLATFORM_FILE_ERROR_TOO_MANY_OPENED;
          break;
        case ENOMEM:
          *error = PLATFORM_FILE_ERROR_NO_MEMORY;
          break;
        case ENOSPC:
          *error = PLATFORM_FILE_ERROR_NO_SPACE;
          break;
        case ENOTDIR:
          *error = PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
          break;
        default:
          *error = PLATFORM_FILE_ERROR_FAILED;
      }
    }
  }

  return descriptor;
}

bool ClosePlatformFile(PlatformFile file) {
  base::ThreadRestrictions::AssertIOAllowed();
  return !HANDLE_EINTR(close(file));
}

int64 SeekPlatformFile(PlatformFile file,
                       PlatformFileWhence whence,
                       int64 offset) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0 || offset < 0)
    return -1;

  return lseek(file, static_cast<off_t>(offset), static_cast<int>(whence));
}

int ReadPlatformFile(PlatformFile file, int64 offset, char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0 || size < 0)
    return -1;

  int bytes_read = 0;
  int rv;
  do {
    rv = HANDLE_EINTR(pread(file, data + bytes_read,
                            size - bytes_read, offset + bytes_read));
    if (rv <= 0)
      break;

    bytes_read += rv;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : rv;
}

int ReadPlatformFileAtCurrentPos(PlatformFile file, char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0 || size < 0)
    return -1;

  int bytes_read = 0;
  int rv;
  do {
    rv = HANDLE_EINTR(read(file, data, size));
    if (rv <= 0)
      break;

    bytes_read += rv;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : rv;
}

int ReadPlatformFileNoBestEffort(PlatformFile file, int64 offset,
                                 char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0)
    return -1;

  return HANDLE_EINTR(pread(file, data, size, offset));
}

int ReadPlatformFileCurPosNoBestEffort(PlatformFile file,
                                       char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0 || size < 0)
    return -1;

  return HANDLE_EINTR(read(file, data, size));
}

int WritePlatformFile(PlatformFile file, int64 offset,
                      const char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0 || size < 0)
    return -1;

  int bytes_written = 0;
  int rv;
  do {
    rv = HANDLE_EINTR(pwrite(file, data + bytes_written,
                             size - bytes_written, offset + bytes_written));
    if (rv <= 0)
      break;

    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : rv;
}

int WritePlatformFileAtCurrentPos(PlatformFile file,
                                  const char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0 || size < 0)
    return -1;

  int bytes_written = 0;
  int rv;
  do {
    rv = HANDLE_EINTR(write(file, data, size));
    if (rv <= 0)
      break;

    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : rv;
}

int WritePlatformFileCurPosNoBestEffort(PlatformFile file,
                                        const char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (file < 0 || size < 0)
    return -1;

  return HANDLE_EINTR(write(file, data, size));
}

bool TruncatePlatformFile(PlatformFile file, int64 length) {
  base::ThreadRestrictions::AssertIOAllowed();
  return ((file >= 0) && !HANDLE_EINTR(ftruncate(file, length)));
}

bool FlushPlatformFile(PlatformFile file) {
  base::ThreadRestrictions::AssertIOAllowed();
  return !HANDLE_EINTR(fsync(file));
}

bool TouchPlatformFile(PlatformFile file, const base::Time& last_access_time,
                       const base::Time& last_modified_time) {
  base::ThreadRestrictions::AssertIOAllowed();
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

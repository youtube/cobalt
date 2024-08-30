// Copyright 2018 Google Inc. All Rights Reserved.
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

// Adapted from platform_file_posix.cc
#include "base/logging.h"
#include "base/files/file_starboard.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "starboard/common/metrics/stats_tracker.h"
#include "starboard/file.h"

namespace base {

void RecordFileWriteStat(int write_file_result) {
  auto& stats_tracker =
      starboard::StatsTrackerContainer::GetInstance()->stats_tracker();
  if (write_file_result <= 0) {
    stats_tracker.FileWriteFail();
  } else {
    stats_tracker.FileWriteSuccess();
    stats_tracker.FileWriteBytesWritten(/*bytes_written=*/write_file_result);
  }
}

// Make sure our Whence mappings match the system headers.
static_assert(File::FROM_BEGIN == static_cast<int>(SEEK_SET) &&
                  File::FROM_CURRENT == static_cast<int>(SEEK_CUR) &&
                  File::FROM_END == static_cast<int>(SEEK_END),
              "Whence enums from base must match those of Starboard.");

void File::Info::FromStat(const stat_wrapper_t& stat_info) {
  is_directory = S_ISDIR(stat_info.st_mode);
  is_symbolic_link = S_ISLNK(stat_info.st_mode);
  size = stat_info.st_size;

  // Get last modification time, last access time, and creation time from
  // |stat_info|.
  // Note: st_ctime is actually last status change time when the inode was last
  // updated, which happens on any metadata change. It is not the file's
  // creation time. However, other than on Mac & iOS where the actual file
  // creation time is included as st_birthtime, the rest of POSIX platforms have
  // no portable way to get the creation time.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  time_t last_modified_sec = stat_info.st_mtim.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtim.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atim.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atim.tv_nsec;
  time_t creation_time_sec = stat_info.st_ctim.tv_sec;
  int64_t creation_time_nsec = stat_info.st_ctim.tv_nsec;
#elif BUILDFLAG(IS_ANDROID)
  time_t last_modified_sec = stat_info.st_mtime;
  int64_t last_modified_nsec = stat_info.st_mtime_nsec;
  time_t last_accessed_sec = stat_info.st_atime;
  int64_t last_accessed_nsec = stat_info.st_atime_nsec;
  time_t creation_time_sec = stat_info.st_ctime;
  int64_t creation_time_nsec = stat_info.st_ctime_nsec;
#elif BUILDFLAG(IS_APPLE)
  time_t last_modified_sec = stat_info.st_mtimespec.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtimespec.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atimespec.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atimespec.tv_nsec;
  time_t creation_time_sec = stat_info.st_birthtimespec.tv_sec;
  int64_t creation_time_nsec = stat_info.st_birthtimespec.tv_nsec;
#elif BUILDFLAG(IS_BSD)
  time_t last_modified_sec = stat_info.st_mtimespec.tv_sec;
  int64_t last_modified_nsec = stat_info.st_mtimespec.tv_nsec;
  time_t last_accessed_sec = stat_info.st_atimespec.tv_sec;
  int64_t last_accessed_nsec = stat_info.st_atimespec.tv_nsec;
  time_t creation_time_sec = stat_info.st_ctimespec.tv_sec;
  int64_t creation_time_nsec = stat_info.st_ctimespec.tv_nsec;
#else
  time_t last_modified_sec = stat_info.st_mtime;
  int64_t last_modified_nsec = 0;
  time_t last_accessed_sec = stat_info.st_atime;
  int64_t last_accessed_nsec = 0;
  time_t creation_time_sec = stat_info.st_ctime;
  int64_t creation_time_nsec = 0;
#endif

  last_modified =
      Time::FromTimeT(last_modified_sec) +
      Microseconds(last_modified_nsec / Time::kNanosecondsPerMicrosecond);

  last_accessed =
      Time::FromTimeT(last_accessed_sec) +
      Microseconds(last_accessed_nsec / Time::kNanosecondsPerMicrosecond);

  creation_time =
      Time::FromTimeT(creation_time_sec) +
      Microseconds(creation_time_nsec / Time::kNanosecondsPerMicrosecond);
}

bool File::IsValid() const {
  return file_.is_valid();
}

PlatformFile File::GetPlatformFile() const {
  return file_.get();
}

PlatformFile File::TakePlatformFile() {
  return file_.release();
}

void File::Close() {
  if (!IsValid())
    return;

  SCOPED_FILE_TRACE("Close");
  internal::AssertBlockingAllowed();
  file_.reset();
}

int64_t File::Seek(Whence whence, int64_t offset) {
  internal::AssertBlockingAllowed();
  DCHECK(IsValid());

  SCOPED_FILE_TRACE_WITH_SIZE("Seek", offset);
  return lseek(file_.get(), static_cast<off_t>(offset),
               static_cast<int>(whence));
}

int File::Read(int64_t offset, char* data, int size) {
  internal::AssertBlockingAllowed();
  DCHECK(IsValid());
  if (size < 0) {
    return -1;
  }

  SCOPED_FILE_TRACE_WITH_SIZE("Read", size);

  int original_position = lseek(file_.get(), 0, static_cast<int>(SEEK_CUR));
  if (original_position < 0) {
    return -1;
  }

  int position =
      lseek(file_.get(), static_cast<off_t>(offset),  static_cast<int>(SEEK_SET));
  int result = 0;
  if (position == offset) {
    result = ReadAtCurrentPos(data, size);
  }

  // Restore position regardless of result of write.
  position =
      lseek(file_.get(), static_cast<off_t>(original_position), static_cast<int>(SEEK_SET));
  if (result < 0) {
    return result;
  }

  if (position < 0) {
    return -1;
  }

  return result;
}

int File::ReadAtCurrentPos(char* data, int size) {
  internal::AssertBlockingAllowed();
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("ReadAtCurrentPos", size);

  return starboard::ReadAll(file_.get(), data, size);
}

int File::ReadNoBestEffort(int64_t offset, char* data, int size) {
  internal::AssertBlockingAllowed();
  DCHECK(IsValid());
  SCOPED_FILE_TRACE_WITH_SIZE("ReadNoBestEffort", size);

  int original_position = lseek(
      file_.get(), 0, static_cast<int>(SEEK_CUR));
  if (original_position < 0) {
    return -1;
  }

  int position =
      lseek(file_.get(), static_cast<off_t>(offset),  static_cast<int>(SEEK_SET));
  int result = 0;
  if (position == offset) {
    result = read(file_.get(), data, size);
  }

  // Restore position regardless of result of read.
  position = lseek(file_.get(), static_cast<off_t>(original_position),
                   static_cast<int>(SEEK_SET));
  if (result < 0) {
    return result;
  }

  if (position < 0) {
    return -1;
  }

  return result;
}

int File::ReadAtCurrentPosNoBestEffort(char* data, int size) {
  internal::AssertBlockingAllowed();
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("ReadAtCurrentPosNoBestEffort", size);
  return read(file_.get(), data, size);
}

int File::Write(int64_t offset, const char* data, int size) {
  internal::AssertBlockingAllowed();
  if (append_) {
    return WriteAtCurrentPos(data, size);
  }

  int original_position = lseek(file_.get(), 0,  static_cast<int>(SEEK_CUR));
  if (original_position < 0) {
    return -1;
  }

  int64_t position =
      lseek(file_.get(), static_cast<off_t>(offset), static_cast<int>(SEEK_SET));

  int result = 0;
  if (position == offset) {
    result = WriteAtCurrentPos(data, size);
  }

  // Restore position regardless of result of write.
  position = lseek(file_.get(), static_cast<off_t>(original_position),
                   static_cast<int>(SEEK_SET));
  if (result < 0) {
    return result;
  }

  if (position < 0) {
    return -1;
  }

  return result;
}

int File::WriteAtCurrentPos(const char* data, int size) {
  internal::AssertBlockingAllowed();
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("WriteAtCurrentPos", size);

  int bytes_written = 0;
  long rv;
  do {
    rv = HANDLE_EINTR(write(file_.get(), data + bytes_written,
                            static_cast<size_t>(size - bytes_written)));
    if (rv <= 0)
      break;

    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : checked_cast<int>(rv);
}

int File::WriteAtCurrentPosNoBestEffort(const char* data, int size) {
  internal::AssertBlockingAllowed();
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("WriteAtCurrentPosNoBestEffort", size);
  int write_result = write(file_.get(), data, size);
  RecordFileWriteStat(write_result);
  return write_result;
}

int64_t File::GetLength() {
  DCHECK(IsValid());

  SCOPED_FILE_TRACE("GetLength");

  stat_wrapper_t file_info;
  if (Fstat(file_.get(), &file_info))
    return -1;

  return file_info.st_size;
}

bool File::SetLength(int64_t length) {
  internal::AssertBlockingAllowed();
  DCHECK(IsValid());
  SCOPED_FILE_TRACE_WITH_SIZE("SetLength", length);
  return !ftruncate(file_.get(), length);
}

bool File::SetTimes(Time last_access_time, Time last_modified_time) {
  internal::AssertBlockingAllowed();
  DCHECK(IsValid());

  SCOPED_FILE_TRACE("SetTimes");

  NOTIMPLEMENTED();
  return false;
}

bool File::GetInfo(Info* info) {
  DCHECK(IsValid());

  SCOPED_FILE_TRACE("GetInfo");

  stat_wrapper_t file_info;
  if (Fstat(file_.get(), &file_info))
    return false;

  info->FromStat(file_info);
  return true;
}

File::Error File::GetLastFileError() {
  return base::File::OSErrorToFileError(errno);
}

// Static.
int File::Stat(const char* path, stat_wrapper_t* sb) {
  internal::AssertBlockingAllowed();
  return stat(path, sb);
}
int File::Fstat(int fd, stat_wrapper_t* sb) {
  internal::AssertBlockingAllowed();
  return fstat(fd, sb);
}

// Static.
File::Error File::OSErrorToFileError(int saved_errno) {
  switch (saved_errno) {
    case EACCES:
    case EISDIR:
    case EROFS:
    case EPERM:
      return FILE_ERROR_ACCESS_DENIED;
    case EBUSY:
#if !BUILDFLAG(IS_NACL)  // ETXTBSY not defined by NaCl.
    case ETXTBSY:
#endif
      return FILE_ERROR_IN_USE;
    case EEXIST:
      return FILE_ERROR_EXISTS;
    case EIO:
      return FILE_ERROR_IO;
    case ENOENT:
      return FILE_ERROR_NOT_FOUND;
    case ENFILE:  // fallthrough
    case EMFILE:
      return FILE_ERROR_TOO_MANY_OPENED;
    case ENOMEM:
      return FILE_ERROR_NO_MEMORY;
    case ENOSPC:
      return FILE_ERROR_NO_SPACE;
    case ENOTDIR:
      return FILE_ERROR_NOT_A_DIRECTORY;
    default:
      // This function should only be called for errors.
      DCHECK_NE(0, saved_errno);
      return FILE_ERROR_FAILED;
  }
}

void File::DoInitialize(const FilePath& path, uint32_t flags) {
  internal::AssertBlockingAllowed();
  DCHECK(!IsValid());

  created_ = false;
  append_ = flags & FLAG_APPEND;
  file_name_ = path.AsUTF8Unsafe();

  int open_flags = 0;
  if (flags & FLAG_CREATE) {
    open_flags = O_CREAT | O_EXCL;
  }

  if (flags & FLAG_CREATE_ALWAYS) {
    SB_DCHECK(!open_flags);
    open_flags = O_CREAT | O_TRUNC;
  }

  if (flags & FLAG_OPEN_TRUNCATED) {
    SB_DCHECK(!open_flags);
    SB_DCHECK(flags & FLAG_WRITE);
    open_flags = O_TRUNC;
  }

  if (!open_flags && !(flags & FLAG_OPEN) && !(flags & FLAG_OPEN_ALWAYS)) {
    SB_NOTREACHED();
    errno = EOPNOTSUPP;
  }

  if (flags & FLAG_WRITE || flags & FLAG_APPEND) {
    if (flags & FLAG_READ) {
      open_flags |= O_RDWR;
    } else {
      open_flags |= O_WRONLY;
    } 
  }

#if defined(O_LARGEFILE)
  // Always add on O_LARGEFILE, regardless of compiler macros
  open_flags |= O_LARGEFILE;
#endif

  SB_COMPILE_ASSERT(O_RDONLY == 0, O_RDONLY_must_equal_zero);

  int mode = S_IRUSR | S_IWUSR;
  int descriptor = HANDLE_EINTR(open(path.value().c_str(), open_flags, mode));

  if (flags & FLAG_OPEN_ALWAYS) {
    if (descriptor < 0) {
      open_flags |= O_CREAT;
      descriptor = HANDLE_EINTR(open(path.value().c_str(), open_flags, mode));
      if (descriptor >= 0)
        created_ = true;
    }
  }

  if (descriptor >= 0 &&
      (flags & (FLAG_CREATE_ALWAYS | FLAG_CREATE))) {
    created_ = true;
  }
  
  file_.reset(descriptor);

  if (!file_.is_valid()) {
#if defined(__ANDROID_API__)
  bool can_read = flags & O_RDONLY;
  bool can_write = flags & O_WRONLY;
  if ((errno == 0) && (!can_read || can_write)) {
    error_details_ = FILE_ERROR_ACCESS_DENIED;
  } else {
    error_details_ = File::GetLastFileError();
  }
#else
  error_details_ = File::GetLastFileError();
#endif
  } else {
    error_details_ = FILE_OK;
    if (append_) {
      lseek(file_.get(), 0, SEEK_END);
    }
  }

  if (flags & FLAG_DELETE_ON_CLOSE) {
    NOTREACHED() << "Not supported on Starboard platforms right now.";
  }

  async_ = ((flags & FLAG_ASYNC) == FLAG_ASYNC);
}

bool File::Flush() {
  internal::AssertBlockingAllowed();
  DCHECK(IsValid());
  SCOPED_FILE_TRACE("Flush");

  return !fsync(file_.get());
}

void File::SetPlatformFile(PlatformFile file) {
  DCHECK(!file_.is_valid());
  file_.reset(file);
}

File File::Duplicate() const {
  NOTREACHED();
  return File();
}

}  // namespace base

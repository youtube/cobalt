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

#include "base/files/file.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "starboard/common/log.h"
#include "starboard/file.h"

namespace base {

// Make sure our Whence mappings match the system headers.
static_assert(
    File::FROM_BEGIN == static_cast<int>(kSbFileFromBegin) &&
    File::FROM_CURRENT == static_cast<int>(kSbFileFromCurrent) &&
    File::FROM_END == static_cast<int>(kSbFileFromEnd),
    "Whence enums from base must match those of Starboard.");

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
  AssertBlockingAllowed();
  file_.reset();
}

int64_t File::Seek(Whence whence, int64_t offset) {
  AssertBlockingAllowed();
  DCHECK(IsValid());

  SCOPED_FILE_TRACE_WITH_SIZE("Seek", offset);
  return SbFileSeek(file_.get(), static_cast<SbFileWhence>(whence), offset);
}

int File::Read(int64_t offset, char* data, int size) {
  AssertBlockingAllowed();
  DCHECK(IsValid());
  if (size < 0) {
    return -1;
  }

  SCOPED_FILE_TRACE_WITH_SIZE("Read", size);

  int original_position = SbFileSeek(file_.get(), kSbFileFromCurrent, 0);
  if (original_position < 0) {
    return -1;
  }

  int position = SbFileSeek(file_.get(), kSbFileFromBegin, offset);
  int result = 0;
  if (position == offset) {
    result = ReadAtCurrentPos(data, size);
  }

  // Restore position regardless of result of write.
  position = SbFileSeek(file_.get(), kSbFileFromBegin, original_position);
  if (result < 0) {
    return result;
  }

  if (position < 0) {
    return -1;
  }

  return result;
}

int File::ReadAtCurrentPos(char* data, int size) {
  AssertBlockingAllowed();
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("ReadAtCurrentPos", size);

  return SbFileReadAll(file_.get(), data, size);
}

int File::ReadNoBestEffort(int64_t offset, char* data, int size) {
  AssertBlockingAllowed();
  DCHECK(IsValid());
  SCOPED_FILE_TRACE_WITH_SIZE("ReadNoBestEffort", size);

  int original_position = SbFileSeek(file_.get(), kSbFileFromCurrent, 0);
  if (original_position < 0) {
    return -1;
  }

  int position = SbFileSeek(file_.get(), kSbFileFromBegin, offset);
  int result = 0;
  if (position == offset) {
    result = SbFileRead(file_.get(), data, size);
  }

  // Restore position regardless of result of read.
  position = SbFileSeek(file_.get(), kSbFileFromBegin, original_position);
  if (result < 0) {
    return result;
  }

  if (position < 0) {
    return -1;
  }

  return result;
}

int File::ReadAtCurrentPosNoBestEffort(char* data, int size) {
  AssertBlockingAllowed();
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("ReadAtCurrentPosNoBestEffort", size);
  return SbFileRead(file_.get(), data, size);
}

int File::Write(int64_t offset, const char* data, int size) {
  AssertBlockingAllowed();

  if (append_) {
    return WriteAtCurrentPos(data, size);
  }

  int original_position = SbFileSeek(file_.get(), kSbFileFromCurrent, 0);
  if (original_position < 0) {
    return -1;
  }

  int64_t position = SbFileSeek(file_.get(), kSbFileFromBegin, offset);
  int result = 0;
  if (position == offset) {
    result = WriteAtCurrentPos(data, size);
  }

  // Restore position regardless of result of write.
  position = SbFileSeek(file_.get(), kSbFileFromBegin, original_position);
  if (result < 0) {
    return result;
  }

  if (position < 0) {
    return -1;
  }

  return result;
}

int File::WriteAtCurrentPos(const char* data, int size) {
  AssertBlockingAllowed();
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("WriteAtCurrentPos", size);

  return SbFileWriteAll(file_.get(), data, size);
}

int File::WriteAtCurrentPosNoBestEffort(const char* data, int size) {
  AssertBlockingAllowed();
  DCHECK(IsValid());
  if (size < 0)
    return -1;

  SCOPED_FILE_TRACE_WITH_SIZE("WriteAtCurrentPosNoBestEffort", size);
  return SbFileWrite(file_.get(), data, size);
}

int64_t File::GetLength() {
  DCHECK(IsValid());

  SCOPED_FILE_TRACE("GetLength");

  File::Info file_info;
  if (!GetInfo(&file_info)) {
    return -1;
  }

  return file_info.size;
}

bool File::SetLength(int64_t length) {
  AssertBlockingAllowed();
  DCHECK(IsValid());

  SCOPED_FILE_TRACE_WITH_SIZE("SetLength", length);
  return SbFileTruncate(file_.get(), length);
}

bool File::SetTimes(Time last_access_time, Time last_modified_time) {
  AssertBlockingAllowed();
  DCHECK(IsValid());

  SCOPED_FILE_TRACE("SetTimes");

  NOTIMPLEMENTED();
  return false;
}

bool File::GetInfo(Info* info) {
  DCHECK(IsValid());

  SCOPED_FILE_TRACE("GetInfo");

  if (!info || !SbFileIsValid(file_.get()))
    return false;

  SbFileInfo file_info;
  if (!SbFileGetInfo(file_.get(), &file_info))
    return false;

  info->is_directory = file_info.is_directory;
  info->is_symbolic_link = file_info.is_symbolic_link;
  info->size = file_info.size;
  info->last_modified = base::Time::FromSbTime(file_info.last_modified);
  info->last_accessed = base::Time::FromSbTime(file_info.last_accessed);
  info->creation_time = base::Time::FromSbTime(file_info.creation_time);
  return true;
}

File::Error File::GetLastFileError() {
  SB_NOTIMPLEMENTED();
  return File::FILE_ERROR_MAX;
}

// Static.
File::Error File::OSErrorToFileError(SbSystemError sb_error) {
  switch (sb_error) {
    case kSbFileErrorFailed:
    case kSbFileErrorInUse:
    case kSbFileErrorExists:
    case kSbFileErrorNotFound:
    case kSbFileErrorAccessDenied:
    case kSbFileErrorTooManyOpened:
    case kSbFileErrorNoMemory:
    case kSbFileErrorNoSpace:
    case kSbFileErrorNotADirectory:
    case kSbFileErrorInvalidOperation:
    case kSbFileErrorSecurity:
    case kSbFileErrorAbort:
    case kSbFileErrorNotAFile:
    case kSbFileErrorNotEmpty:
    case kSbFileErrorInvalidUrl:
    case kSbFileErrorIO:
      // Starboard error codes are designed to match Chromium's exactly.
      return static_cast<File::Error>(sb_error);
      break;
    default:
      NOTREACHED() << "Unrecognized SbFileError: " << sb_error;
      break;
  }
  return FILE_ERROR_FAILED;
}

void File::DoInitialize(const FilePath& path, uint32_t flags) {
  AssertBlockingAllowed();
  DCHECK(!IsValid());

  created_ = false;
  append_ = flags & FLAG_APPEND;

  int open_flags = 0;
  switch (flags & (FLAG_OPEN | FLAG_CREATE |
                   FLAG_OPEN_ALWAYS | FLAG_CREATE_ALWAYS |
                   FLAG_OPEN_TRUNCATED)) {
    case FLAG_OPEN:
      open_flags = kSbFileOpenOnly;
      break;

    case FLAG_CREATE:
      open_flags = kSbFileCreateOnly;
      break;

    case FLAG_OPEN_ALWAYS:
      open_flags = kSbFileOpenAlways;
      break;

    case FLAG_CREATE_ALWAYS:
      open_flags = kSbFileCreateAlways;
      break;

    case FLAG_OPEN_TRUNCATED:
      open_flags = kSbFileOpenTruncated;
      DCHECK(flags & FLAG_WRITE);
      break;

    default:
      NOTREACHED() << "Passed incompatible flags: " << flags;
      error_details_ = FILE_ERROR_FAILED;
  }

  DCHECK(flags & FLAG_WRITE || flags & FLAG_READ || flags & FLAG_APPEND);

  if (flags & FLAG_READ) {
    open_flags |= kSbFileRead;
  }

  if (flags & FLAG_WRITE || flags & FLAG_APPEND) {
    open_flags |= kSbFileWrite;
  }

  SbFileError sb_error;
  file_.reset(
      SbFileOpen(path.value().c_str(), open_flags, &created_, &sb_error));

  if (!file_.is_valid()) {
    error_details_ = OSErrorToFileError(sb_error);
  } else {
    error_details_ = FILE_OK;
    if (append_) {
      SbFileSeek(file_.get(), kSbFileFromEnd, 0);
    }
  }

  if (flags & FLAG_DELETE_ON_CLOSE) {
    NOTREACHED() << "Not supported on Starboard platforms right now.";
  }

  async_ = ((flags & FLAG_ASYNC) == FLAG_ASYNC);
}

bool File::Flush() {
  AssertBlockingAllowed();
  DCHECK(IsValid());
  SCOPED_FILE_TRACE("Flush");

  return SbFileFlush(file_.get());
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

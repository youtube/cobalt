// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/platform_file.h"

#include "base/file_path.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "starboard/file.h"

namespace base {

// Make sure our Whence mappings match the system headers.
COMPILE_ASSERT(
    PLATFORM_FILE_FROM_BEGIN == static_cast<int>(kSbFileFromBegin) &&
    PLATFORM_FILE_FROM_CURRENT == static_cast<int>(kSbFileFromCurrent) &&
    PLATFORM_FILE_FROM_END == static_cast<int>(kSbFileFromEnd),
    whence_matches_system);

PlatformFile CreatePlatformFileUnsafe(const FilePath& name,
                                      int flags,
                                      bool *created,
                                      PlatformFileError* error) {
  base::ThreadRestrictions::AssertIOAllowed();

  int open_flags = 0;

  switch (flags & (PLATFORM_FILE_OPEN | PLATFORM_FILE_CREATE |
                   PLATFORM_FILE_OPEN_ALWAYS | PLATFORM_FILE_CREATE_ALWAYS |
                   PLATFORM_FILE_OPEN_TRUNCATED)) {
    case PLATFORM_FILE_OPEN:
      open_flags = kSbFileOpenOnly;
      break;

    case PLATFORM_FILE_CREATE:
      open_flags = kSbFileCreateOnly;
      break;

    case PLATFORM_FILE_OPEN_ALWAYS:
      open_flags = kSbFileOpenAlways;
      break;

    case PLATFORM_FILE_CREATE_ALWAYS:
      open_flags = kSbFileCreateAlways;
      break;

    case PLATFORM_FILE_OPEN_TRUNCATED:
      open_flags = kSbFileOpenTruncated;
      DCHECK(flags & PLATFORM_FILE_WRITE);
      break;

    default:
      NOTREACHED() << "Passed incompatible flags: " << flags;
      if (error) {
        *error = PLATFORM_FILE_ERROR_FAILED;
      }
      return kInvalidPlatformFileValue;
  }

  DCHECK(flags & PLATFORM_FILE_WRITE || flags & PLATFORM_FILE_READ);

  if (flags & PLATFORM_FILE_READ) {
    open_flags |= kSbFileRead;
  }

  if (flags & PLATFORM_FILE_WRITE) {
    open_flags |= kSbFileWrite;
  }

  SbFileError sb_error;
  SbFile file = SbFileOpen(name.value().c_str(), open_flags, created,
                           &sb_error);

  if (error) {
    if (SbFileIsValid(file)) {
      *error = PLATFORM_FILE_OK;
    } else {
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
          // Starboard error codes are designed to match Chromium's exactly.
          *error = static_cast<PlatformFileError>(sb_error);
          break;
        default:
          NOTREACHED() << "Unrecognized SbFileError: " << sb_error;
          break;
      }
    }
  }

  return file;
}

bool ClosePlatformFile(PlatformFile file) {
  base::ThreadRestrictions::AssertIOAllowed();
  return SbFileClose(file);
}

int64 SeekPlatformFile(PlatformFile file,
                       PlatformFileWhence whence,
                       int64 offset) {
  base::ThreadRestrictions::AssertIOAllowed();
  return SbFileSeek(file, static_cast<SbFileWhence>(whence), offset);
}

int ReadPlatformFile(PlatformFile file, int64 offset, char* data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (size < 0) {
    return -1;
  }

  int original_position = SbFileSeek(file, kSbFileFromCurrent, 0);
  if (original_position < 0) {
    return -1;
  }

  int position = SbFileSeek(file, kSbFileFromBegin, offset);
  int result = 0;
  if (position == offset) {
    result = ReadPlatformFileAtCurrentPos(file, data, size);
  }

  // Restore position regardless of result of write.
  position = SbFileSeek(file, kSbFileFromBegin, original_position);
  if (result < 0) {
    return result;
  }

  if (position < 0) {
    return -1;
  }

  return result;
}

int ReadPlatformFileAtCurrentPos(PlatformFile file, char *data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  return SbFileReadAll(file, data, size);
}

int ReadPlatformFileNoBestEffort(PlatformFile file,
                                 int64 offset,
                                 char *data,
                                 int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  int original_position = SbFileSeek(file, kSbFileFromCurrent, 0);
  if (original_position < 0) {
    return -1;
  }

  int position = SbFileSeek(file, kSbFileFromBegin, offset);
  int result = 0;
  if (position == offset) {
    result = SbFileRead(file, data, size);
  }

  // Restore position regardless of result of read.
  position = SbFileSeek(file, kSbFileFromBegin, original_position);
  if (result < 0) {
    return result;
  }

  if (position < 0) {
    return -1;
  }

  return result;
}

int ReadPlatformFileCurPosNoBestEffort(PlatformFile file,
                                       char *data,
                                       int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  return SbFileRead(file, data, size);
}

int WritePlatformFile(PlatformFile file,
                      int64 offset,
                      const char *data,
                      int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (size < 0) {
    return -1;
  }

  int original_position = SbFileSeek(file, kSbFileFromCurrent, 0);
  if (original_position < 0) {
    return -1;
  }

  int position = SbFileSeek(file, kSbFileFromBegin, offset);
  int result = 0;
  if (position == offset) {
    result = WritePlatformFileAtCurrentPos(file, data, size);
  }

  // Restore position regardless of result of write.
  position = SbFileSeek(file, kSbFileFromBegin, original_position);
  if (result < 0) {
    return result;
  }

  if (position < 0) {
    return -1;
  }

  return result;
}

int WritePlatformFileAtCurrentPos(PlatformFile file,
                                  const char* data,
                                  int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  return SbFileWriteAll(file, data, size);
}

int WritePlatformFileCurPosNoBestEffort(PlatformFile file,
                                        const char *data, int size) {
  base::ThreadRestrictions::AssertIOAllowed();
  if (size < 0) {
    return -1;
  }

  return SbFileWrite(file, data, size);
}

bool TruncatePlatformFile(PlatformFile file, int64 length) {
  base::ThreadRestrictions::AssertIOAllowed();
  return SbFileTruncate(file, length);
}

bool FlushPlatformFile(PlatformFile file) {
  base::ThreadRestrictions::AssertIOAllowed();
  return SbFileFlush(file);
}

bool GetPlatformFileInfo(PlatformFile file, PlatformFileInfo *info) {
  if (!info || !SbFileIsValid(file))
    return false;

  SbFileInfo file_info;
  if (!SbFileGetInfo(file, &file_info))
    return false;

  info->is_directory = file_info.is_directory;
  info->is_symbolic_link = file_info.is_symbolic_link;
  info->size = file_info.size;
  info->last_modified = base::Time::FromSbTime(file_info.last_modified);
  info->last_accessed = base::Time::FromSbTime(file_info.last_accessed);
  info->creation_time = base::Time::FromSbTime(file_info.creation_time);
  return true;
}

}  // namespace base

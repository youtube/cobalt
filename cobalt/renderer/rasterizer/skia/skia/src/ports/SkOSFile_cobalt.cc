/*
 * Copyright 2013 The Cobalt Authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkOSFile_cobalt.h"

#include <fcntl.h>
#include <unistd.h>

#include "SkString.h"
#include "SkTFitsIn.h"
#include "SkTemplates.h"
#include "SkTypes.h"
#include "base/files/file_path.h"
#include "base/files/file_starboard.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "starboard/common/file.h"
#include "starboard/common/file_wrapper.h"

// Implement functionality declared in SkOSFile.h via primitives provided
// by Chromium.  In doing this, we need only ensure that support for Chromium
// file utilities is working and then Skia file utilities will also work.

namespace {

FilePtr ToFilePtr(FILE* sk_file) {
  // PlatformFile is a pointer type in Starboard, so we cannot use static_cast
  // from intptr_t.
  return reinterpret_cast<FilePtr>(sk_file);
}

FILE* ToFILE(FilePtr starboard_file) {
  return reinterpret_cast<FILE*>(starboard_file);
}

int ToFileFlags(SkFILE_Flags sk_flags) {
  int flags = 0;
  if (sk_flags & kRead_SkFILE_Flag) {
    if (sk_flags & kWrite_SkFILE_Flag) {
      flags |= O_WRONLY;
    }
    flags |= O_RDONLY;
  } else if (sk_flags & kWrite_SkFILE_Flag) {
    flags |= O_CREAT | O_WRONLY;
  }
  return flags;
}

}  // namespace

FILE* sk_fopen(const char path[], SkFILE_Flags sk_flags) {
  FilePtr file = file_open(path, ToFileFlags(sk_flags));
  if (!file || file->fd < 0) {
    return nullptr;
  }

  return ToFILE(file);
}

void sk_fclose(FILE* sk_file) {
  SkASSERT(sk_file);
  int ret = file_close(ToFilePtr(sk_file));
}

size_t sk_fgetsize(FILE* sk_file) {
  SkASSERT(sk_file);
  FilePtr file = ToFilePtr(sk_file);

  // Save current position so we can restore it.
  int64_t current_position = lseek(file->fd, 0, SEEK_CUR);
  if (current_position < 0) {
    return 0;
  }

  // Find the file size by seeking to the end.
  int64_t size = lseek(file->fd, 0, SEEK_END);
  if (size < 0) {
    size = 0;
  }

  // Restore original file position.
  lseek(file->fd, current_position, SEEK_SET);
  return size;
}

size_t sk_fwrite(const void* buffer, size_t byteCount, FILE* sk_file) {
  SkASSERT(sk_file);
  FilePtr file = ToFilePtr(sk_file);
  int result =
      write(file->fd, reinterpret_cast<const char*>(buffer), byteCount);
  base::RecordFileWriteStat(result);
  return result;
}

void sk_fflush(FILE* sk_file) {
  SkASSERT(sk_file);
  FilePtr file = ToFilePtr(sk_file);
  fsync(file->fd);
}

bool sk_fseek(FILE* sk_file, size_t position) {
  SkASSERT(sk_file);
  FilePtr file = ToFilePtr(sk_file);
  int64_t new_position = lseek(file->fd, position, SEEK_SET);
  return new_position == position;
}

size_t sk_ftell(FILE* sk_file) {
  SkASSERT(sk_file);
  FilePtr file = ToFilePtr(sk_file);
  return lseek(file->fd, 0, SEEK_CUR);
}

void* sk_fmmap(FILE* sk_file, size_t* length) {
  // Not supported, but clients may try to call to see if it is supported.
  return NULL;
}

void* sk_fdmmap(int fd, size_t* length) {
  NOTREACHED() << __FUNCTION__;
  return NULL;
}

void sk_fmunmap(const void* addr, size_t length) {
  NOTREACHED() << __FUNCTION__;
}

bool sk_fidentical(FILE* sk_file_a, FILE* sk_file_b) {
  NOTREACHED() << __FUNCTION__;
  return false;
}

int sk_fileno(FILE* sk_file_a) {
  NOTREACHED() << __FUNCTION__;
  return -1;
}

bool sk_exists(const char* path, SkFILE_Flags) {
  return base::PathExists(base::FilePath(path));
}

bool sk_isdir(const char* path) {
  return base::DirectoryExists(base::FilePath(path));
}

bool sk_mkdir(const char* path) {
  // Strange linking error on windows when calling base::CreateDirectory,
  // having something to do with a system #define. This isn't used, anyway.
  NOTREACHED() << __FUNCTION__;
  return false;
}

void sk_fsync(FILE* f) {
  SkASSERT(f);
  FilePtr file = ToFilePtr(f);
  // Technically, flush doesn't have to call sync... but this is the best
  // effort we can make.
  fsync(file->fd);
}

size_t sk_qread(FILE* file, void* buffer, size_t count, size_t offset) {
  SkASSERT(file);
  FilePtr starboard_file = ToFilePtr(file);

  int original_position = lseek(starboard_file->fd, 0, SEEK_CUR);
  if (original_position < 0) {
    return SIZE_MAX;
  }

  int position = lseek(starboard_file->fd, offset, SEEK_SET);
  int result = 0;
  if (position == offset) {
    result =
        starboard::ReadAll(starboard_file->fd, reinterpret_cast<char*>(buffer),
                           static_cast<int>(count));
  }
  position = lseek(starboard_file->fd, original_position, SEEK_SET);
  if (result < 0 || position < 0) {
    return SIZE_MAX;
  } else {
    return result;
  }
}

size_t sk_fread(void* buffer, size_t byteCount, FILE* file) {
  SkASSERT(file);
  FilePtr starboard_file = ToFilePtr(file);
  return starboard::ReadAll(starboard_file->fd, reinterpret_cast<char*>(buffer),
                            byteCount);
}

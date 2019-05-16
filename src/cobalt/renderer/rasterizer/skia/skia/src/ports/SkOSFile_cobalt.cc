/*
 * Copyright 2013 The Cobalt Authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkOSFile_cobalt.h"

#include "SkString.h"
#include "SkTFitsIn.h"
#include "SkTemplates.h"
#include "SkTypes.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"

// Implement functionality declared in SkOSFile.h via primitives provided
// by Chromium.  In doing this, we need only ensure that support for Chromium
// file utilities is working and then Skia file utilities will also work.

namespace {

SbFile ToSbFile(SkFile* sk_file) {
  // PlatformFile is a pointer type in Starboard, so we cannot use static_cast
  // from intptr_t.
  return reinterpret_cast<SbFile>(sk_file);
}

SkFile* ToFILE(SbFile starboard_file) {
  return reinterpret_cast<SkFile*>(starboard_file);
}

int ToSbFileFlags(SkFILE_Flags sk_flags) {
  int flags = 0;
  if (sk_flags & kRead_SkFILE_Flag) {
    if (sk_flags & kWrite_SkFILE_Flag) {
      flags |= kSbFileWrite;
    }
    flags |= kSbFileOpenOnly | kSbFileRead;
  } else if (sk_flags & kWrite_SkFILE_Flag) {
    flags |= kSbFileOpenAlways | kSbFileWrite;
  }
  return flags;
}

}  // namespace

SkFile* sk_fopen(const char path[], SkFILE_Flags sk_flags) {
  SbFile starboard_file = SbFileOpen(path, ToSbFileFlags(sk_flags), NULL, NULL);
  if (starboard_file == base::kInvalidPlatformFile) {
    return nullptr;
  }

  return ToFILE(starboard_file);
}

void sk_fclose(SkFile* sk_file) {
  SkASSERT(sk_file);
  SbFileClose(ToSbFile(sk_file));
}

size_t sk_fgetsize(SkFile* sk_file) {
  SkASSERT(sk_file);
  SbFile file = ToSbFile(sk_file);

  // Save current position so we can restore it.
  int64_t current_position = SbFileSeek(file, kSbFileFromCurrent, 0);
  if (current_position < 0) {
    return 0;
  }

  // Find the file size by seeking to the end.
  int64_t size = SbFileSeek(file, kSbFileFromEnd, 0);
  if (size < 0) {
    size = 0;
  }

  // Restore original file position.
  SbFileSeek(file, kSbFileFromBegin, current_position);
  return size;
}

size_t sk_fwrite(const void* buffer, size_t byteCount, SkFile* sk_file) {
  SkASSERT(sk_file);
  SbFile file = ToSbFile(sk_file);
  return SbFileWrite(file, reinterpret_cast<const char*>(buffer), byteCount);
}

void sk_fflush(SkFile* sk_file) {
  SkASSERT(sk_file);
  SbFile file = ToSbFile(sk_file);
  SbFileFlush(file);
}

bool sk_fseek(SkFile* sk_file, size_t position) {
  SkASSERT(sk_file);
  SbFile file = ToSbFile(sk_file);
  int64_t new_position = SbFileSeek(file, kSbFileFromBegin, position);
  return new_position == position;
}

size_t sk_ftell(SkFile* sk_file) {
  SkASSERT(sk_file);
  SbFile file = ToSbFile(sk_file);
  return SbFileSeek(file, kSbFileFromCurrent, 0);
}

void* sk_fmmap(SkFile* sk_file, size_t* length) {
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

bool sk_fidentical(SkFile* sk_file_a, SkFile* sk_file_b) {
  NOTREACHED() << __FUNCTION__;
  return false;
}

int sk_fileno(SkFile* sk_file_a) {
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

void sk_fsync(SkFile* f) {
  SkASSERT(f);
  SbFile file = ToSbFile(f);
  // Technically, flush doesn't have to call sync... but this is the best
  // effort we can make.
  SbFileFlush(file);
}

size_t sk_qread(SkFile* file, void* buffer, size_t count, size_t offset) {
  SkASSERT(file);
  SbFile starboard_file = ToSbFile(file);

  int original_position = SbFileSeek(starboard_file, kSbFileFromCurrent, 0);
  if (original_position < 0) {
    return SIZE_MAX;
  }

  int position = SbFileSeek(starboard_file, kSbFileFromBegin, offset);
  int result = 0;
  if (position == offset) {
    result = SbFileReadAll(starboard_file, reinterpret_cast<char*>(buffer),
                           static_cast<int>(count));
  }
  position = SbFileSeek(starboard_file, kSbFileFromBegin, original_position);
  if (result < 0 || position < 0) {
    return SIZE_MAX;
  } else {
    return result;
  }
}

size_t sk_fread(void* buffer, size_t byteCount, SkFile* file) {
  SkASSERT(file);
  SbFile starboard_file = ToSbFile(file);
  return SbFileReadAll(starboard_file, reinterpret_cast<char*>(buffer),
                       byteCount);
}

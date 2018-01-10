/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkOSFile_cobalt.h"

#include "SkString.h"
#include "SkTFitsIn.h"
#include "SkTemplates.h"
#include "SkTypes.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"

// Implement functionality declared in SkOSFile.h via primitives provided
// by Chromium.  In doing this, we need only ensure that support for Chromium
// file utilities is working and then Skia file utilities will also work.

namespace {

base::PlatformFile ToPlatformFile(SkFile* sk_file) {
  // PlatformFile is a pointer type in Starboard, so we cannot use static_cast
  // from intptr_t.
  return reinterpret_cast<base::PlatformFile>(sk_file);
}

SkFile* ToFILE(base::PlatformFile platform_file) {
  return reinterpret_cast<SkFile*>(platform_file);
}

int ToPlatformFlags(SkFILE_Flags sk_flags) {
  int flags = 0;
  if (sk_flags & kRead_SkFILE_Flag) {
    if (sk_flags & kWrite_SkFILE_Flag) {
      flags |= base::PLATFORM_FILE_WRITE;
    }
    flags |= base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ;
  } else if (sk_flags & kWrite_SkFILE_Flag) {
    flags |= base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE;
  }
  return flags;
}

}  // namespace

SkFile* sk_fopen(const char path[], SkFILE_Flags sk_flags) {
  base::PlatformFile platform_file = base::CreatePlatformFile(
      FilePath(path), ToPlatformFlags(sk_flags), NULL, NULL);
  if (platform_file == base::kInvalidPlatformFileValue) {
    return nullptr;
  }

  return ToFILE(platform_file);
}

void sk_fclose(SkFile* sk_file) {
  SkASSERT(sk_file);
  base::ClosePlatformFile(ToPlatformFile(sk_file));
}

size_t sk_fgetsize(SkFile* sk_file) {
  SkASSERT(sk_file);
  base::PlatformFile file = ToPlatformFile(sk_file);

  // Save current position so we can restore it.
  int64_t current_position =
      SeekPlatformFile(file, base::PLATFORM_FILE_FROM_CURRENT, 0);
  if (current_position < 0) {
    return 0;
  }

  // Find the file size by seeking to the end.
  int64_t size = base::SeekPlatformFile(file, base::PLATFORM_FILE_FROM_END, 0);
  if (size < 0) {
    size = 0;
  }

  // Restore original file position.
  SeekPlatformFile(file, base::PLATFORM_FILE_FROM_BEGIN, current_position);
  return size;
}

size_t sk_fwrite(const void* buffer, size_t byteCount, SkFile* sk_file) {
  SkASSERT(sk_file);
  base::PlatformFile file = ToPlatformFile(sk_file);
  return base::WritePlatformFileAtCurrentPos(
      file, reinterpret_cast<const char*>(buffer), byteCount);
}

void sk_fflush(SkFile* sk_file) {
  SkASSERT(sk_file);
  base::PlatformFile file = ToPlatformFile(sk_file);
  base::FlushPlatformFile(file);
}

bool sk_fseek(SkFile* sk_file, size_t position) {
  SkASSERT(sk_file);
  base::PlatformFile file = ToPlatformFile(sk_file);
  int64_t new_position =
      SeekPlatformFile(file, base::PLATFORM_FILE_FROM_BEGIN, position);
  return new_position == position;
}

size_t sk_ftell(SkFile* sk_file) {
  SkASSERT(sk_file);
  base::PlatformFile file = ToPlatformFile(sk_file);
  return SeekPlatformFile(file, base::PLATFORM_FILE_FROM_CURRENT, 0);
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
  return file_util::PathExists(FilePath(path));
}

bool sk_isdir(const char* path) {
  return file_util::DirectoryExists(FilePath(path));
}

bool sk_mkdir(const char* path) {
  // Strange linking error on windows when calling file_util::CreateDirectory,
  // having something to do with a system #define. This isn't used, anyway.
  NOTREACHED() << __FUNCTION__;
  return false;
}

void sk_fsync(SkFile* f) {
  SkASSERT(f);
  base::PlatformFile file = ToPlatformFile(f);
  // Technically, flush doesn't have to call sync... but this is the best
  // effort we can make.
  base::FlushPlatformFile(file);
}

size_t sk_qread(SkFile* file, void* buffer, size_t count, size_t offset) {
  SkASSERT(file);
  base::PlatformFile platform_file = ToPlatformFile(file);
  return base::ReadPlatformFile(platform_file, offset,
                                reinterpret_cast<char*>(buffer), count);
}

size_t sk_fread(void* buffer, size_t byteCount, SkFile* file) {
  SkASSERT(file);
  base::PlatformFile platform_file = ToPlatformFile(file);
  return base::ReadPlatformFileAtCurrentPos(
      platform_file, reinterpret_cast<char*>(buffer), byteCount);
}

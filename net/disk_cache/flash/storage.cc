// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/flash/storage.h"

#include <fcntl.h>

#include "base/logging.h"
#include "base/platform_file.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/flash/format.h"

namespace disk_cache {

Storage::Storage(const FilePath& path, int32 size) : path_(path), size_(size) {
  COMPILE_ASSERT(kFlashPageSize % 2 == 0, invalid_page_size);
  COMPILE_ASSERT(kFlashBlockSize % kFlashPageSize == 0, invalid_block_size);
  DCHECK(size_ % kFlashBlockSize == 0);
}

bool Storage::Init() {
  int flags = base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_OPEN_ALWAYS;

  file_ = base::CreatePlatformFile(path_, flags, NULL, NULL);
  if (file_ == base::kInvalidPlatformFileValue)
    return false;

  // TODO(agayev): if file already exists, do some validation and return either
  // true or false based on the result.

#if defined(OS_LINUX)
  fallocate(file_, 0, 0, size_);
#endif

  return true;
}

Storage::~Storage() {
  base::ClosePlatformFile(file_);
}

bool Storage::Read(void* buffer, int32 size, int32 offset) {
  DCHECK(offset >= 0 && offset + size <= size_);

  int rv = base::ReadPlatformFile(file_, offset, static_cast<char*>(buffer),
                                  size);
  return rv == size;
}

bool Storage::Write(const void* buffer, int32 size, int32 offset) {
  DCHECK(offset >= 0 && offset + size <= size_);

  int rv = base::WritePlatformFile(file_, offset,
                                   static_cast<const char*>(buffer), size);
  return rv == size;
}

}  // namespace disk_cache

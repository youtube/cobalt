// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/mapped_file.h"

#include <errno.h>
#include <sys/mman.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "net/disk_cache/disk_cache.h"

namespace disk_cache {

void* MappedFile::Init(const base::FilePath& name, size_t size) {
  return nullptr;
}

void MappedFile::Flush() {
}

MappedFile::~MappedFile() {
}

}  // namespace disk_cache

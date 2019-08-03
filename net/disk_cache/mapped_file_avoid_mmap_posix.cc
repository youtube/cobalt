// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/mapped_file.h"

#include <stdlib.h>

#include "base/file_path.h"
#include "base/logging.h"

namespace disk_cache {

void* MappedFile::Init(const FilePath& name, size_t size) {
  DCHECK(!init_);
  if (init_ || !File::Init(name))
    return NULL;

  if (!size)
    size = GetLength();

  buffer_ = malloc(size);
  snapshot_ = malloc(size);
  if (buffer_ && snapshot_ && Read(buffer_, size, 0)) {
    memcpy(snapshot_, buffer_, size);
  } else {
    free(buffer_);
    free(snapshot_);
    buffer_ = snapshot_ = 0;
  }

  init_ = true;
  view_size_ = size;
  return buffer_;
}

bool MappedFile::Load(const FileBlock* block) {
  size_t offset = block->offset() + view_size_;
  return Read(block->buffer(), block->size(), offset);
}

bool MappedFile::Store(const FileBlock* block) {
  size_t offset = block->offset() + view_size_;
  return Write(block->buffer(), block->size(), offset);
}

void MappedFile::Flush() {
  DCHECK(buffer_);
  DCHECK(snapshot_);
  if (0 == memcmp(buffer_, snapshot_, view_size_)) {
    // Nothing changed, no need to flush.
    return;
  }

  const char* buffer_ptr = static_cast<const char*>(buffer_);
  char* snapshot_ptr = static_cast<char*>(snapshot_);
  size_t i = 0;
  while (i < view_size_) {
    size_t run_start = i;
    // Look for a run of changed bytes (possibly zero-sized). Write them out.
    while(i < view_size_ && snapshot_ptr[i] != buffer_ptr[i]) {
      snapshot_ptr[i] = buffer_ptr[i];
      i++;
    }
    if (i > run_start) {
      Write(snapshot_ptr + run_start, i - run_start, run_start);
    }
    // Look for a run of unchanged bytes (possibly zero-sized). Skip them.
    while (i < view_size_ && snapshot_ptr[i] == buffer_ptr[i]) {
      i++;
    }
  }
  DCHECK(0 == memcmp(buffer_, snapshot_, view_size_));
}

MappedFile::~MappedFile() {
  if (!init_)
    return;

  if (buffer_ && snapshot_) {
    Flush();
  }
  free(buffer_);
  free(snapshot_);
}

}  // namespace disk_cache

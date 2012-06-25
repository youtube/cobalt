// Copyright (c) 2012 Google Inc. All rights reserved.
// This file is very similar to the posix version except this implementation
// doesn't use memory mapped file.

#include "net/disk_cache/mapped_file.h"

#include "base/file_path.h"
#include "base/logging.h"

namespace disk_cache {

void* MappedFile::Init(const FilePath& name, size_t size) {
  DCHECK(!init_);
  if (init_ || !File::Init(name))
    return NULL;

  if (!size) size = GetLength();

  // Create buffer
  buffer_ = new char[size];

  if (Read(buffer_, size, 0)) {
    // Read data into buffer
    view_size_ = size;
    init_ = true;
  } else {
    // Reset
    view_size_ = 0;
    delete [] buffer_;
    buffer_ = NULL;
  }
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

MappedFile::~MappedFile() {
  if (init_) {
    // update file
    Write(buffer_, view_size_, 0);
    delete [] buffer_;
    buffer_ = NULL;
  }
}

}  // namespace disk_cache

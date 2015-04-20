/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
    delete [] static_cast<char*>(buffer_);
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

void MappedFile::Flush() {
}

MappedFile::~MappedFile() {
  if (init_) {
    // update file
    Write(buffer_, view_size_, 0);
    delete [] static_cast<char*>(buffer_);
    buffer_ = NULL;
  }
}

}  // namespace disk_cache

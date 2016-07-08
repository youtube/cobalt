/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/storage/virtual_file.h"

#include <algorithm>

#include "base/logging.h"

namespace cobalt {
namespace storage {
namespace {

// We must use forward declarations to be able to specify the
// WARN_UNUSED_RESULT attribute.

// Read size bytes from buffer into dest. Return # of bytes read.
int ReadBuffer(uint8* dest, const uint8* buffer, int size) WARN_UNUSED_RESULT;

// Write size bytes from source into buffer. Return # of bytes written.
int WriteBuffer(uint8* buffer, const uint8* source, int size,
                bool dry_run) WARN_UNUSED_RESULT;

int ReadBuffer(uint8* dest, const uint8* buffer, int size) {
  memcpy(dest, buffer, static_cast<size_t>(size));
  return size;
}

int WriteBuffer(uint8* buffer, const uint8* source, int size, bool dry_run) {
  if (!dry_run) {
    memcpy(buffer, source, static_cast<size_t>(size));
  }
  return size;
}

}  // namespace

VirtualFile::VirtualFile(const std::string& name) : size_(0), name_(name) {}
VirtualFile::~VirtualFile() {}

int VirtualFile::Read(void* dest, int bytes, int offset) const {
  DCHECK_GE(offset, 0);
  if (offset > size_) {
    return 0;
  }
  int bytes_to_read = std::min(size_ - offset, bytes);
  if (bytes_to_read == 0) {
    return 0;
  }
  memcpy(dest, &buffer_[static_cast<size_t>(offset)],
         static_cast<size_t>(bytes_to_read));
  return bytes_to_read;
}

int VirtualFile::Write(const void* source, int bytes, int offset) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(bytes, 0);
  if (static_cast<int>(buffer_.size()) < offset + bytes) {
    buffer_.resize(static_cast<size_t>(offset + bytes));
  }

  memcpy(&buffer_[static_cast<size_t>(offset)], source,
         static_cast<size_t>(bytes));
  size_ = std::max(size_, offset + bytes);
  return bytes;
}

int VirtualFile::Truncate(int size) {
  size_ = std::min(size_, size);
  return size_;
}

int VirtualFile::Serialize(uint8* dest, bool dry_run) {
  uint8* cur_buffer = dest;

  // Write filename length
  uint64 name_length = name_.length();
  DCHECK_LT(name_length, kMaxVfsPathname);
  cur_buffer +=
      WriteBuffer(cur_buffer, reinterpret_cast<const uint8*>(&name_length),
                  sizeof(name_length), dry_run);

  // Write the filename
  cur_buffer +=
      WriteBuffer(cur_buffer, reinterpret_cast<const uint8*>(name_.c_str()),
                  static_cast<int>(name_length), dry_run);

  // NOTE: Ensure the file size is 64-bit for compatibility
  // with any existing serialized files.
  uint64 file_size = static_cast<uint64>(size_);
  // Write the file contents size
  cur_buffer +=
      WriteBuffer(cur_buffer, reinterpret_cast<const uint8*>(&file_size),
                  sizeof(file_size), dry_run);

  // Write the file contents
  cur_buffer += WriteBuffer(cur_buffer, &buffer_[0], size_, dry_run);

  // Return the number of bytes written
  return static_cast<int>(std::distance(dest, cur_buffer));
}

int VirtualFile::Deserialize(const uint8* source) {
  // Read in filename length
  const uint8* cur_buffer = source;
  uint64 name_length;
  cur_buffer += ReadBuffer(reinterpret_cast<uint8*>(&name_length), cur_buffer,
                           sizeof(name_length));

  if (name_length >= kMaxVfsPathname) {
    DLOG(ERROR) << "Filename was longer than the maximum allowed.";
    return -1;
  }
  // Read in filename
  char name[kMaxVfsPathname];
  cur_buffer += ReadBuffer(reinterpret_cast<uint8*>(name), cur_buffer,
                           static_cast<int>(name_length));
  name_.assign(name, name_length);

  // Read in file contents size.
  uint64 file_size;
  cur_buffer += ReadBuffer(reinterpret_cast<uint8*>(&file_size), cur_buffer,
                           sizeof(file_size));
  size_ = static_cast<int>(file_size);

  // Read in the file contents
  buffer_.resize(static_cast<size_t>(size_));
  cur_buffer += ReadBuffer(&buffer_[0], cur_buffer, size_);

  // Return the number of bytes read
  return static_cast<int>(std::distance(source, cur_buffer));
}

}  // namespace storage
}  // namespace cobalt

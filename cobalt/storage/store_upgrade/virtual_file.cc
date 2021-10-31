// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/storage/store_upgrade/virtual_file.h"

#include <algorithm>

#include "base/logging.h"

namespace cobalt {
namespace storage {
namespace store_upgrade {
namespace {

// We must use forward declarations to be able to specify the
// WARN_UNUSED_RESULT attribute.

// Read size bytes from buffer into dest. Updates buffer and buffer_remaining
// on success.
// Returns true on success, false on buffer overrun
bool ReadBuffer(uint8* dest, const uint8** buffer, size_t size,
                size_t* buffer_remaining) WARN_UNUSED_RESULT;

// Write size bytes from source into buffer. Return # of bytes written.
size_t WriteBuffer(uint8* buffer, const uint8* source, size_t size,
                   bool dry_run) WARN_UNUSED_RESULT;

bool ReadBuffer(uint8* dest, const uint8** buffer, size_t size,
                size_t* buffer_remaining) {
  if (size > *buffer_remaining) {
    return false;
  }
  memcpy(dest, *buffer, size);
  *buffer_remaining -= size;
  *buffer += size;
  return true;
}

size_t WriteBuffer(uint8* buffer, const uint8* source, size_t size,
                   bool dry_run) {
  if (!dry_run) {
    memcpy(buffer, source, size);
  }
  return size;
}

}  // namespace

VirtualFile::VirtualFile(const std::string& name) : name_(name) {}
VirtualFile::~VirtualFile() {}

int VirtualFile::Read(void* dest, int bytes_in, int offset_in) const {
  DCHECK_GE(offset_in, 0);
  DCHECK_GE(bytes_in, 0);
  size_t offset = static_cast<size_t>(offset_in);
  size_t bytes = static_cast<size_t>(bytes_in);
  size_t size = buffer_.size();
  if (offset > size) {
    return 0;
  }
  size_t bytes_to_read = std::min(static_cast<size_t>(size - offset), bytes);
  if (bytes_to_read == 0) {
    return 0;
  }
  memcpy(dest, &buffer_[offset], bytes_to_read);
  return static_cast<int>(bytes_to_read);
}

int VirtualFile::Write(const void* source, int bytes_in, int offset_in) {
  DCHECK_GE(offset_in, 0);
  DCHECK_GE(bytes_in, 0);
  size_t bytes = static_cast<size_t>(bytes_in);
  size_t offset = static_cast<size_t>(offset_in);
  if (buffer_.size() < offset + bytes) {
    buffer_.resize(offset + bytes);
  }

  if (!buffer_.empty()) {
    // std::vector does not define access to underlying array when empty
    memcpy(&buffer_[offset], source, bytes);
  }
  return bytes_in;
}

int VirtualFile::Truncate(int size) {
  size_t newsize = std::min(buffer_.size(), static_cast<size_t>(size));
  buffer_.resize(newsize);
  return static_cast<int>(newsize);
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
                  static_cast<size_t>(name_length), dry_run);

  // NOTE: Ensure the file size is 64-bit for compatibility
  // with any existing serialized files.
  uint64 file_size = static_cast<uint64>(buffer_.size());
  // Write the file contents size
  cur_buffer +=
      WriteBuffer(cur_buffer, reinterpret_cast<const uint8*>(&file_size),
                  sizeof(file_size), dry_run);

  // Write the file contents
  if (!buffer_.empty()) {
    // std::vector does not define access to underlying array when empty
    cur_buffer += WriteBuffer(cur_buffer, &buffer_[0], buffer_.size(), dry_run);
  }

  // Return the number of bytes written
  return static_cast<int>(std::distance(dest, cur_buffer));
}

int VirtualFile::Deserialize(const uint8* source, size_t buffer_remaining) {
  // Read in filename length
  const uint8* cur_buffer = source;
  uint64 name_length;
  bool success = ReadBuffer(reinterpret_cast<uint8*>(&name_length), &cur_buffer,
                            sizeof(name_length), &buffer_remaining);
  if (!success) {
    DLOG(ERROR) << "Buffer overrun";
    return -1;
  }

  if (name_length >= kMaxVfsPathname) {
    DLOG(ERROR) << "Filename was longer than the maximum allowed.";
    return -1;
  }
  // Read in filename
  char name[kMaxVfsPathname];
  success = ReadBuffer(reinterpret_cast<uint8*>(name), &cur_buffer,
                       static_cast<size_t>(name_length), &buffer_remaining);
  if (!success) {
    DLOG(ERROR) << "Buffer overrun";
    return -1;
  }
  name_.assign(name, static_cast<size_t>(name_length));

  // Read in file contents size.
  uint64 file_size;
  success = ReadBuffer(reinterpret_cast<uint8*>(&file_size), &cur_buffer,
                       sizeof(file_size), &buffer_remaining);
  if (!success) {
    DLOG(ERROR) << "Buffer overrun";
    return -1;
  }
  // Read in the file contents
  buffer_.resize(static_cast<size_t>(file_size));

  if (!buffer_.empty()) {
    // std::vector does not define access to underlying array when empty
    success =
        ReadBuffer(&buffer_[0], &cur_buffer, buffer_.size(), &buffer_remaining);
    if (!success) {
      DLOG(ERROR) << "Buffer overrun";
      return -1;
    }
  }

  // Return the number of bytes read
  return static_cast<int>(std::distance(source, cur_buffer));
}

}  // namespace store_upgrade
}  // namespace storage
}  // namespace cobalt

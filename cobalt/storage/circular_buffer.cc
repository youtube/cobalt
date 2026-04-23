// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/storage/circular_buffer.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstring>

#include "base/logging.h"

namespace cobalt {
namespace storage {

namespace {
const uint32_t kMagic = 0x43425546;  // "CBUF"
const uint32_t kVersion = 1;
}  // namespace

CircularBuffer::CircularBuffer(const std::string& path, int64_t size)
    : path_(path), total_size_(size) {}

CircularBuffer::~CircularBuffer() {
  if (mmapped_address_ && mmapped_address_ != MAP_FAILED) {
    if (munmap(mmapped_address_, total_size_) != 0) {
      PLOG(ERROR) << "Failed to munmap";
    }
  }
}

bool CircularBuffer::Init() {
  int fd = open(path_.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    PLOG(ERROR) << "Failed to open file: " << path_;
    return false;
  }

  // Ensure file is of correct size before mapping.
  if (ftruncate(fd, total_size_) != 0) {
    PLOG(ERROR) << "Failed to truncate file: " << path_;
    close(fd);
    return false;
  }

  mmapped_address_ =
      mmap(nullptr, total_size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mmapped_address_ == MAP_FAILED) {
    PLOG(ERROR) << "Failed to mmap file: " << path_;
    close(fd);
    return false;
  }

  close(fd);  // OK to close fd after mmap

  header_ = reinterpret_cast<Header*>(mmapped_address_);
  data_ = reinterpret_cast<char*>(mmapped_address_) + sizeof(Header);
  data_size_ = total_size_ - sizeof(Header);

  if (header_->magic != kMagic) {
    LOG(INFO) << "Initializing new circular buffer file.";
    header_->magic = kMagic;
    header_->version = kVersion;
    header_->head = 0;
    header_->tail = 0;
    header_->size = data_size_;
  } else {
    LOG(INFO) << "Found existing circular buffer file. Head: " << header_->head
              << ", Tail: " << header_->tail;
    if (header_->size != data_size_) {
      LOG(WARNING) << "File data size mismatch! Expected: " << data_size_
                   << ", Found: " << header_->size;
      // Handle resize if needed, but for now just fail or reset.
      return false;
    }
  }

  return true;
}

bool CircularBuffer::Append(const std::string& data) {
  if (!mmapped_address_) {
    LOG(ERROR) << "CircularBuffer not initialized.";
    return false;
  }

  uint32_t length = data.size();
  uint32_t total_needed = sizeof(length) + length;

  if (total_needed > data_size_) {
    LOG(ERROR) << "Data too large for buffer.";
    return false;
  }

  uint32_t tail = header_->tail;
  uint32_t head = header_->head;

  // Check if write would wrap around
  if (tail + total_needed > data_size_) {
    // Write marker if space permits
    if (tail + sizeof(uint32_t) <= data_size_) {
      uint32_t marker = 0xFFFFFFFF;
      std::memcpy(data_ + tail, &marker, sizeof(marker));
    }
    tail = 0;
  }

  // Check if we need to advance head.
  bool empty = (head == tail);

  if (!empty) {
    while (true) {
      bool conflict = false;
      if (tail < head) {
        if (tail + total_needed >= head) {
          conflict = true;
        }
      }

      if (!conflict) {
        break;
      }

      // Advance head to next event
      uint32_t current_event_length;
      std::memcpy(&current_event_length, data_ + head,
                  sizeof(current_event_length));

      if (current_event_length == 0xFFFFFFFF) {
        head = 0;
        std::memcpy(&current_event_length, data_ + head,
                    sizeof(current_event_length));
      }

      head = (head + sizeof(current_event_length) + current_event_length) %
             data_size_;

      if (head == header_->tail) {
        // Buffer becomes empty
        break;
      }
    }
    header_->head = head;
  }

  // Write length
  std::memcpy(data_ + tail, &length, sizeof(length));
  // Write data
  std::memcpy(data_ + tail + sizeof(length), data.data(), length);

  header_->tail = (tail + total_needed) % data_size_;

  if (msync(mmapped_address_, total_size_, MS_ASYNC) != 0) {
    PLOG(ERROR) << "Failed to msync";
  }

  return true;
}

bool CircularBuffer::Peek(std::string* data) {
  if (!mmapped_address_) {
    LOG(ERROR) << "CircularBuffer not initialized.";
    return false;
  }

  uint32_t head = header_->head;
  uint32_t tail = header_->tail;

  if (head == tail) {
    return false;  // Empty
  }

  uint32_t length;
  std::memcpy(&length, data_ + head, sizeof(length));

  if (length == 0xFFFFFFFF) {
    head = 0;
    std::memcpy(&length, data_ + head, sizeof(length));
  }

  if (head == tail) {
    return false;  // Empty after wrap
  }

  data->assign(data_ + head + sizeof(length), length);

  return true;
}

bool CircularBuffer::Pop() {
  if (!mmapped_address_) {
    LOG(ERROR) << "CircularBuffer not initialized.";
    return false;
  }

  uint32_t head = header_->head;
  uint32_t tail = header_->tail;

  if (head == tail) {
    return false;  // Empty, nothing to commit
  }

  uint32_t length;
  std::memcpy(&length, data_ + head, sizeof(length));

  if (length == 0xFFFFFFFF) {
    head = 0;
    std::memcpy(&length, data_ + head, sizeof(length));
  }

  header_->head = (head + sizeof(length) + length) % data_size_;

  return true;
}

}  // namespace storage
}  // namespace cobalt

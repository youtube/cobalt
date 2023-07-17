// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "nb/multipart_allocator.h"

#include <algorithm>

#include "starboard/common/log.h"
#include "starboard/memory.h"

namespace nb {

MultipartAllocator::Allocations::Allocations(void* buffer, int buffer_size)
    : number_of_buffers_(1),
      buffers_(&buffer_),
      buffer_sizes_(&buffer_size_),
      buffer_(buffer),
      buffer_size_(buffer_size) {
  SB_DCHECK(buffer != NULL);
  SB_DCHECK(buffer_size > 0);
}

MultipartAllocator::Allocations::Allocations(int number_of_buffers,
                                             void** buffers,
                                             const int* buffer_sizes)
    : number_of_buffers_(0), buffers_(NULL), buffer_sizes_(NULL) {
  SB_DCHECK(number_of_buffers > 0);
  SB_DCHECK(buffers != NULL);
  SB_DCHECK(buffer_sizes != NULL);

  Assign(number_of_buffers, buffers, buffer_sizes);

  for (int i = 0; i < number_of_buffers_; ++i) {
    SB_DCHECK(buffers_[i] != NULL);
    SB_DCHECK(buffer_sizes_[i] > 0);
  }
}

MultipartAllocator::Allocations::Allocations(const Allocations& that)
    : number_of_buffers_(0), buffers_(NULL), buffer_sizes_(NULL) {
  Assign(that.number_of_buffers_, that.buffers_, that.buffer_sizes_);
}

MultipartAllocator::Allocations::~Allocations() {
  Destroy();
}

MultipartAllocator::Allocations& MultipartAllocator::Allocations::operator=(
    const Allocations& that) {
  Destroy();
  Assign(that.number_of_buffers_, that.buffers_, that.buffer_sizes_);
  return *this;
}

int MultipartAllocator::Allocations::size() const {
  int size = 0;

  for (size_t i = 0; i < number_of_buffers_; ++i) {
    size += buffer_sizes_[i];
  }

  return size;
}

void MultipartAllocator::Allocations::ShrinkTo(int size) {
  for (size_t i = 0; i < number_of_buffers_; ++i) {
    if (size >= buffer_sizes_[i]) {
      size -= buffer_sizes_[i];
    } else {
      buffer_sizes_[i] = size;
      size = 0;
      ++i;
      while (i < number_of_buffers_) {
        buffer_sizes_[i] = 0;
        ++i;
      }
      break;
    }
  }

  SB_DCHECK(size == 0);
}

void MultipartAllocator::Allocations::Write(int destination_offset,
                                            const void* src,
                                            int size) {
  size_t buffer_index = 0;
  const uint8_t* src_in_uint8 = static_cast<const uint8_t*>(src);
  while (size > 0) {
    if (buffer_index >= number_of_buffers_) {
      SB_NOTREACHED();
      return;
    }
    if (buffer_sizes_[buffer_index] <= destination_offset) {
      destination_offset -= buffer_sizes_[buffer_index];
    } else {
      int bytes_to_copy =
          std::min(size, buffer_sizes_[buffer_index] - destination_offset);
      uint8_t* destination_in_uint8 =
          static_cast<uint8_t*>(buffers_[buffer_index]);
      memcpy(destination_in_uint8 + destination_offset, src_in_uint8,
                   bytes_to_copy);
      destination_offset = 0;
      src_in_uint8 += bytes_to_copy;
      size -= bytes_to_copy;
    }
    ++buffer_index;
  }
}

void MultipartAllocator::Allocations::Read(void* destination) const {
  uint8_t* destination_in_uint8 = static_cast<uint8_t*>(destination);

  for (size_t i = 0; i < number_of_buffers_; ++i) {
    memcpy(destination_in_uint8, buffers_[i], buffer_sizes_[i]);
    destination_in_uint8 += buffer_sizes_[i];
  }
}

void MultipartAllocator::Allocations::Assign(int number_of_buffers,
                                             void** buffers,
                                             const int* buffer_sizes) {
  SB_DCHECK(number_of_buffers_ == 0);
  SB_DCHECK(buffers_ == NULL);
  SB_DCHECK(buffer_sizes_ == NULL);

  number_of_buffers_ = number_of_buffers;
  if (number_of_buffers_ == 0) {
    buffers_ = NULL;
    buffer_sizes_ = NULL;
    return;
  }
  if (number_of_buffers_ == 1) {
    buffers_ = &buffer_;
    buffer_sizes_ = &buffer_size_;
    buffer_ = buffers[0];
    buffer_size_ = buffer_sizes[0];
    return;
  }
  buffers_ = new void*[number_of_buffers_];
  buffer_sizes_ = new int[number_of_buffers_];
  memcpy(buffers_, buffers, sizeof(*buffers_) * number_of_buffers_);
  memcpy(buffer_sizes_, buffer_sizes,
               sizeof(*buffer_sizes_) * number_of_buffers_);
}

void MultipartAllocator::Allocations::Destroy() {
  if (number_of_buffers_ > 1) {
    delete[] buffers_;
    delete[] buffer_sizes_;
  }
  number_of_buffers_ = 0;
  buffers_ = NULL;
  buffer_sizes_ = NULL;
}

}  // namespace nb

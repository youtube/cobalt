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
#include "starboard/log.h"
#include "starboard/memory.h"

namespace nb {

MultipartAllocator::Allocations::Allocations(void* buffer, int size) {
  SB_DCHECK(size > 0);

  buffers_.push_back(buffer);
  buffer_sizes_.push_back(size);
}

MultipartAllocator::Allocations::Allocations(
    const std::vector<void*>& buffers,
    const std::vector<int>& buffer_sizes)
    : buffers_(buffers), buffer_sizes_(buffer_sizes) {
  SB_DCHECK(buffers_.size() == buffer_sizes_.size());
  for (size_t i = 0; i < buffers_.size(); ++i) {
    SB_DCHECK(buffers_[i] != NULL);
  }
}

int MultipartAllocator::Allocations::size() const {
  int size = 0;

  for (size_t i = 0; i < buffers_.size(); ++i) {
    size += buffer_sizes_[i];
  }

  return size;
}

void MultipartAllocator::Allocations::ShrinkTo(int size) {
  for (size_t i = 0; i < buffers_.size(); ++i) {
    if (size >= buffer_sizes_[i]) {
      size -= buffer_sizes_[i];
    } else {
      buffer_sizes_[i] = size;
      size = 0;
      ++i;
      while (i < buffers_.size()) {
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
    if (buffer_index >= buffers_.size()) {
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
      SbMemoryCopy(destination_in_uint8 + destination_offset, src_in_uint8,
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

  for (size_t i = 0; i < buffers_.size(); ++i) {
    SbMemoryCopy(destination_in_uint8, buffers_[i], buffer_sizes_[i]);
    destination_in_uint8 += buffer_sizes_[i];
  }
}

}  // namespace nb

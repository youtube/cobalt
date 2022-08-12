// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/xhr/fetch_buffer_pool.h"

#include <algorithm>
#include <utility>

namespace cobalt {
namespace xhr {

int FetchBufferPool::GetSize() const {
  int size = 0;
  for (const auto& buffer : buffers_) {
    size += buffer.byte_length();
  }

  // The last buffer may not be fully populated.
  if (current_buffer_) {
    DCHECK_EQ(current_buffer_, &buffers_.back());
    DCHECK_LE(current_buffer_offset_, current_buffer_->byte_length());

    auto remaining = current_buffer_->byte_length() - current_buffer_offset_;
    size -= remaining;
  }

  DCHECK_GE(size, 0);
  return size;
}

void FetchBufferPool::Clear() {
  buffers_.clear();
  current_buffer_ = nullptr;
  current_buffer_offset_ = 0;
}

int FetchBufferPool::Write(const void* data, int num_bytes) {
  DCHECK(num_bytes >= 0);

  if (num_bytes > 0) {
    DCHECK(data);
  }

  auto source_bytes_remaining = num_bytes;
  while (source_bytes_remaining > 0) {
    EnsureCurrentBufferAllocated(source_bytes_remaining);

    int destination_bytes_remaining =
        current_buffer_->byte_length() - current_buffer_offset_;
    int bytes_to_copy =
        std::min(destination_bytes_remaining, source_bytes_remaining);

    memcpy(current_buffer_->data() + current_buffer_offset_, data,
           bytes_to_copy);

    data = static_cast<const uint8_t*>(data) + bytes_to_copy;
    current_buffer_offset_ += bytes_to_copy;
    source_bytes_remaining -= bytes_to_copy;

    if (current_buffer_offset_ == current_buffer_->byte_length()) {
      current_buffer_ = nullptr;
      current_buffer_offset_ = 0;
    }
  }

  return num_bytes;
}

void FetchBufferPool::ResetAndReturnAsArrayBuffers(
    bool return_all,
    std::vector<script::PreallocatedArrayBufferData>* buffers) {
  DCHECK(buffers);

  buffers->clear();

  if (!return_all && buffers_.size() > 1 && current_buffer_) {
    // Don't return the last buffer when:
    //   1. `return_all` is set to false
    //   2. the last buffer is not full (current_buffer_ is not null)
    //   3. There are more than one buffer cached so the caller will receive
    //      something.
    auto saved_last_buffer = std::move(buffers_.back());
    buffers_.pop_back();
    buffers->swap(buffers_);
    buffers_.push_back(std::move(saved_last_buffer));
    current_buffer_ = &buffers_.back();
    return;
  }

  if (current_buffer_) {
    DCHECK(return_all || buffers_.size() == 1);
    current_buffer_->Resize(current_buffer_offset_);
  }

  buffers->swap(buffers_);

  Clear();
}

void FetchBufferPool::EnsureCurrentBufferAllocated(int expected_buffer_size) {
  if (current_buffer_ != nullptr) {
    DCHECK_EQ(current_buffer_, &buffers_.back());
    DCHECK_LE(current_buffer_offset_, current_buffer_->byte_length());
    return;
  }

  buffers_.emplace_back(std::max(default_buffer_size_, expected_buffer_size));

  current_buffer_ = &buffers_.back();
  current_buffer_offset_ = 0;
}

}  // namespace xhr
}  // namespace cobalt

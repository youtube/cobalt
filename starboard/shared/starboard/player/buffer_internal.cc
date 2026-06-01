// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/buffer_internal.h"

#include <cstring>
#include <mutex>
#include <vector>

#include "starboard/common/once.h"
#include "starboard/shared/starboard/player/buffer_pool.h"

#if BUILDFLAG(IS_ANDROID)
#include "starboard/shared/starboard/features.h"
#endif

namespace starboard {
namespace {

// kBufferSize (32KB) is chosen to be large enough to hold the maximum typical
// float32 PCM audio frame payloads:
// - Stereo Float32 PCM at 48kHz (20ms packets) requires 7,680 bytes.
// - 5.1 Surround Float32 PCM at 48kHz (20ms packets) requires 23,040 bytes.
constexpr size_t kBufferSize = 32'768;

// kPoolSize (128) provides an abundant safety margin. Real-world playback
// tests show a maximum of ~23 buffers in-flight concurrently (leaving 105+
// free), while consuming only a negligible 4MB of RAM total (32KB * 128).
constexpr size_t kPoolSize = 128;

#if defined(NDEBUG)
constexpr int kPaddingSize = 0;
#else   // defined(NDEBUG)
constexpr uint8_t kPadding = 0x78;
constexpr int kPaddingSize = 32;
#endif  // defined(NDEBUG)

BufferPool* GetBufferPool() {
  static BufferPool* instance = new BufferPool(kBufferSize, kPoolSize);
  return instance;
}

}  // namespace

Buffer::Buffer(int size) : size_(size) {
  SB_CHECK_GE(size, 0);
  if (size_ == 0) {
    data_ = nullptr;
    is_pooled_ = false;
    return;
  }

  size_t total_size = static_cast<size_t>(size) + kPaddingSize * 2;
  data_ = nullptr;
  if (UseBufferPool() && total_size <= kBufferSize) {
    data_ = GetBufferPool()->Allocate(total_size);
  }
  is_pooled_ = (data_ != nullptr);
  if (!data_) {
    data_ = new uint8_t[total_size];
  }
#if !defined(NDEBUG)
  memset(data_, kPadding, kPaddingSize);
  memset(data_ + kPaddingSize + size_, kPadding, kPaddingSize);
#endif  // !defined(NDEBUG)
}

Buffer::Buffer(const Buffer& that) : Buffer(that.data_ ? that.size_ : 0) {
  if (!data_) {
    return;
  }
  size_t total_size = static_cast<size_t>(size_) + kPaddingSize * 2;
  memcpy(data_, that.data_, total_size);
}

Buffer::Buffer(Buffer&& that)
    : size_(that.size_), data_(that.data_), is_pooled_(that.is_pooled_) {
  that.size_ = 0;
  that.data_ = nullptr;
  that.is_pooled_ = false;
}

Buffer::~Buffer() {
#if !defined(NDEBUG)
  if (data_) {
    uint8_t buffer[kPaddingSize];
    memset(buffer, kPadding, kPaddingSize);
    SB_CHECK_EQ(memcmp(data_, buffer, kPaddingSize), 0);
    SB_CHECK_EQ(memcmp(data_ + kPaddingSize + size_, buffer, kPaddingSize), 0);
  }
#endif  // !defined(NDEBUG)
  if (is_pooled_) {
    GetBufferPool()->Free(data_, size_ + kPaddingSize * 2);
  } else {
    delete[] data_;
  }
}

Buffer& Buffer::operator=(Buffer&& that) {
  if (this == &that) {
    return *this;
  }

  {
    // This moves the resource to temp, and temp is destroyed
    // immediately at the closing brace, releasing the memory.
    Buffer temp(std::move(*this));
  }

  size_ = that.size_;
  data_ = that.data_;
  is_pooled_ = that.is_pooled_;

  that.size_ = 0;
  that.data_ = nullptr;
  that.is_pooled_ = false;
  return *this;
}

uint8_t* Buffer::data() {
  return size_ == 0 ? nullptr : data_ + kPaddingSize;
}

const uint8_t* Buffer::data() const {
  return size_ == 0 ? nullptr : data_ + kPaddingSize;
}

// static
bool Buffer::UseBufferPool() {
#if BUILDFLAG(IS_ANDROID)
  return starboard::features::FeatureList::IsEnabled(
      starboard::features::kEnableDecodedAudioBufferPool);
#else
  return false;
#endif
}

}  // namespace starboard

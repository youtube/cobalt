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

#include <mutex>
#include <vector>

#include "starboard/common/once.h"

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

// BufferPool is a thread-safe, fixed-size memory pool used to allocate and
// reuse physical memory buffers for audio PCM data. This reduces heap churn
// during playback. The pool is managed as a singleton with a lifetime spanning
// the duration of the application.
class BufferPool {
 public:
  static BufferPool* Get();

  uint8_t* Allocate(size_t size) {
    SB_CHECK_LE(size, kBufferSize);
    std::lock_guard lock(mutex_);
    if (!free_list_.empty()) {
      uint8_t* ptr = free_list_.back();
      free_list_.pop_back();
      return ptr;
    }
    return new uint8_t[size];
  }

  void Free(uint8_t* ptr, size_t size) {
    SB_CHECK_LE(size, kBufferSize);
    if (IsFromPool(ptr)) {
      std::lock_guard lock(mutex_);
      SB_CHECK_LT(free_list_.size(), kPoolSize);
      free_list_.push_back(ptr);
    } else {
      delete[] ptr;
    }
  }

 private:
  BufferPool() : pool_storage_(new uint8_t[kBufferSize * kPoolSize]) {
    for (size_t i = 0; i < kPoolSize; ++i) {
      free_list_.push_back(pool_storage_ + i * kBufferSize);
    }
    SB_LOG(INFO) << "BufferPool is created: buffer size=" << kBufferSize
                 << ", pool size=" << kPoolSize;
  }

  bool IsFromPool(uint8_t* ptr) const {
    return ptr >= pool_storage_ &&
           ptr < pool_storage_ + kBufferSize * kPoolSize;
  }

  uint8_t* pool_storage_;
  std::vector<uint8_t*> free_list_;
  std::mutex mutex_;

  BufferPool(const BufferPool&) = delete;
  void operator=(const BufferPool&) = delete;
};

SB_ONCE_INITIALIZE_FUNCTION(BufferPool, BufferPool::Get)

}  // namespace

Buffer::Buffer(int size) : size_(size) {
  SB_CHECK_GE(size, 0);
  static bool s_config_logged = []() {
    SB_LOG(WARNING) << "STATS_CONFIG: Buffer initialized! kUseBufferPool = "
                    << (UseBufferPool() ? "true" : "false")
                    << ", kBufferSize = " << kBufferSize;
    return true;
  }();
  (void)s_config_logged;
  if (size_ == 0) {
    data_ = nullptr;
    is_pooled_ = false;
    return;
  }

  size_t total_size = static_cast<size_t>(size) + kPaddingSize * 2;
  if (UseBufferPool() && total_size <= kBufferSize) {
    data_ = BufferPool::Get()->Allocate(total_size);
    is_pooled_ = true;
  } else {
    data_ = new uint8_t[total_size];
    is_pooled_ = false;
  }
#if !defined(NDEBUG)
  memset(data_, kPadding, kPaddingSize);
  memset(data_ + kPaddingSize + size_, kPadding, kPaddingSize);
#endif  // !defined(NDEBUG)
}

Buffer::Buffer(const Buffer& that) : size_(that.size_) {
  if (!that.data_) {
    data_ = nullptr;
    is_pooled_ = false;
    return;
  }
  size_t total_size = static_cast<size_t>(size_) + kPaddingSize * 2;
  if (UseBufferPool() && total_size <= kBufferSize) {
    data_ = BufferPool::Get()->Allocate(total_size);
    is_pooled_ = true;
  } else {
    data_ = new uint8_t[total_size];
    is_pooled_ = false;
  }
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
    BufferPool::Get()->Free(data_, size_ + kPaddingSize * 2);
  } else {
    delete[] data_;
  }
}

Buffer& Buffer::operator=(Buffer&& that) {
  std::swap(this->size_, that.size_);
  std::swap(this->data_, that.data_);
  std::swap(this->is_pooled_, that.is_pooled_);
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

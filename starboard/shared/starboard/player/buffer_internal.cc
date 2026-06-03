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

#include "starboard/shared/starboard/player/buffer_internal.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "starboard/common/no_destructor.h"
#include "starboard/shared/starboard/player/pooled_allocator.h"

namespace starboard {
namespace {

// Two-Tiered Bounded Pool (Total pre-allocated memory: 960 KiB < 1 MB):
// - Small Pool (8 KiB blocks, 40 blocks = 320 KiB): For Opus packets
// - Large Pool (80 KiB blocks, 8 blocks = 640 KiB): For WSOLA/Renderer pull
constexpr size_t kSmallBlockSize = 8 * 1024;
constexpr size_t kSmallPoolBlocks = 40;
constexpr size_t kLargeBlockSize = 80 * 1024;
constexpr size_t kLargePoolBlocks = 8;

std::atomic<PooledAllocator*> g_audio_allocator_ptr{nullptr};

PooledAllocator* GetDecodedBufferAllocator() {
  static PooledAllocator* allocator_ptr = [] {
    static NoDestructor<PooledAllocator> allocator(
        std::vector<PooledAllocator::PoolConfig>{
            {"AudioPool_Small", kSmallBlockSize, kSmallPoolBlocks},
            {"AudioPool_Large", kLargeBlockSize, kLargePoolBlocks}});
    PooledAllocator* a = allocator.get();
    g_audio_allocator_ptr.store(a, std::memory_order_release);
    return a;
  }();
  return allocator_ptr;
}

}  // namespace

std::atomic<bool> Buffer::s_pool_enabled{false};

void Buffer::SetPoolEnabled(bool enabled) {
  s_pool_enabled.store(enabled, std::memory_order_relaxed);
}

uint8_t* Buffer::AllocateData(size_t size) {
  if (s_pool_enabled.load(std::memory_order_relaxed)) {
    return static_cast<uint8_t*>(GetDecodedBufferAllocator()->Allocate(size));
  }
  return static_cast<uint8_t*>(::operator new(size));
}

void Buffer::FreeData(uint8_t* ptr) {
  if (!ptr) {
    return;
  }

  PooledAllocator* allocator =
      g_audio_allocator_ptr.load(std::memory_order_acquire);
  if (allocator) {
    allocator->Free(ptr);
  } else {
    ::operator delete(ptr);
  }
}

}  // namespace starboard

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

#include "media/starboard/decoder_buffer_allocator.h"

#include "media/base/demuxer_stream.h"
#include "starboard/common/experimental/media_buffer_pool.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(b/369245553): Remove this file once Finch is ready, as we no longer need
// to support switching strategy on the fly.

namespace media {
namespace {

using starboard::common::experimental::IsPointerAnnotated;
using starboard::common::experimental::MediaBufferPool;

TEST(DecoderBufferAllocatorStrategyTest, SwitchStrategyImmediatelyWhenIdle) {
  if (!MediaBufferPool::Acquire()) {
    return;
  }

  DecoderBufferAllocator allocator;
  const size_t alignment = allocator.GetBufferAlignment();

  // Allocate and free to ensure a strategy is created and then idle.
  auto handle = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_FALSE(IsPointerAnnotated(reinterpret_cast<void*>(handle)));
  EXPECT_GT(allocator.GetAllocatedMemory(), 0u);

  allocator.Free(DemuxerStream::VIDEO, handle, 1024);
  EXPECT_EQ(allocator.GetAllocatedMemory(), 0u);

  // Enable media buffer pool strategy. It should immediately reset the current
  // strategy.
  allocator.EnableMediaBufferPoolStrategy();
  EXPECT_EQ(allocator.GetAllocatedMemory(), 0u);

  // Next allocation should recreate a strategy (potentially the new one).
  handle = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_TRUE(IsPointerAnnotated(reinterpret_cast<void*>(handle)));

  EXPECT_GT(allocator.GetAllocatedMemory(), 0u);
  allocator.Free(DemuxerStream::VIDEO, handle, 1024);
}

TEST(DecoderBufferAllocatorStrategyTest, SwitchStrategyPendingWhenBusy) {
  if (!MediaBufferPool::Acquire()) {
    return;
  }

  DecoderBufferAllocator allocator;
  const size_t alignment = allocator.GetBufferAlignment();

  // Allocate to make it busy.
  auto handle_1 = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle_1, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_FALSE(IsPointerAnnotated(reinterpret_cast<void*>(handle_1)));
  EXPECT_GT(allocator.GetAllocatedMemory(), 0u);

  // Enable media buffer pool strategy. It should enter kPendingEnabling and NOT
  // reset yet.
  allocator.EnableMediaBufferPoolStrategy();
  // The metrics get into a gray area, so we don't verify their values.  But
  // they shouldn't crash.
  allocator.GetCurrentMemoryCapacity();
  allocator.GetAllocatedMemory();

  auto handle_2 = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle_2, DecoderBuffer::Allocator::kInvalidHandle);

  // Free the allocations. This should trigger the reset.
  allocator.Free(DemuxerStream::VIDEO, handle_1, 1024);
  allocator.Free(DemuxerStream::VIDEO, handle_2, 1024);

  // Next allocation should recreate a strategy.
  auto handle_3 = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle_3, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_GT(allocator.GetAllocatedMemory(), 0u);
  allocator.Free(DemuxerStream::VIDEO, handle_3, 1024);
}

TEST(DecoderBufferAllocatorStrategyTest, RepeatedEnableDoesNothing) {
  if (!MediaBufferPool::Acquire()) {
    return;
  }

  DecoderBufferAllocator allocator;
  const size_t alignment = allocator.GetBufferAlignment();

  // Allocate and free to ensure a strategy is created and then idle.
  auto handle = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_FALSE(IsPointerAnnotated(reinterpret_cast<void*>(handle)));
  EXPECT_GT(allocator.GetAllocatedMemory(), 0u);

  allocator.Free(DemuxerStream::VIDEO, handle, 1024);
  EXPECT_EQ(allocator.GetAllocatedMemory(), 0u);

  // Enable media buffer pool strategy. It should immediately reset the current
  // strategy.
  allocator.EnableMediaBufferPoolStrategy();
  EXPECT_EQ(allocator.GetAllocatedMemory(), 0u);

  // Next allocation should recreate a strategy (potentially the new one).
  handle = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_TRUE(IsPointerAnnotated(reinterpret_cast<void*>(handle)));

  // This is no-op.
  allocator.EnableMediaBufferPoolStrategy();

  EXPECT_GT(allocator.GetAllocatedMemory(), 0u);
  allocator.Free(DemuxerStream::VIDEO, handle, 1024);
}

}  // namespace
}  // namespace media

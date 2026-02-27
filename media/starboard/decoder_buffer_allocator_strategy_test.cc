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
#include "media/starboard/bidirectional_fit_decoder_buffer_allocator_strategy.h"
#include "starboard/common/in_place_reuse_allocator_base.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(b/369245553): Remove this file once Finch is ready, as we no longer need
// to support switching strategy on the fly.

namespace media {
namespace {

using InPlaceReuseAllocatorStrategy =
    BidirectionalFitDecoderBufferAllocatorStrategy<
        starboard::common::InPlaceReuseAllocatorBase>;

// Helper function that updates the allocator's strategy.
// The provided callback increments `creation_count` each time a new strategy
// is instantiated, allowing tests to verify exactly when and how many times
// the strategy creation logic is triggered.
void UpdateStrategyWithCreationCounter(DecoderBufferAllocator* allocator,
                                       int* creation_count) {
  allocator->UpdateAllocatorStrategy(base::BindRepeating(
      [](int* creation_count_ptr, int initial_capacity, int allocation_unit)
          -> std::unique_ptr<DecoderBufferAllocator::Strategy> {
        (*creation_count_ptr)++;
        return std::make_unique<InPlaceReuseAllocatorStrategy>(initial_capacity,
                                                               allocation_unit);
      },
      base::Unretained(creation_count)));
}

TEST(DecoderBufferAllocatorStrategyTest, SwitchStrategyImmediatelyWhenIdle) {
  DecoderBufferAllocator allocator;
  const size_t alignment = allocator.GetBufferAlignment();

  int creation_count_a = 0;
  int creation_count_b = 0;

  UpdateStrategyWithCreationCounter(&allocator, &creation_count_a);

  // Allocate and free to ensure a strategy is created and then idle.
  auto handle = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_EQ(creation_count_a, 1);
  EXPECT_EQ(creation_count_b, 0);

  allocator.Free(DemuxerStream::VIDEO, handle, 1024);

  // Switch strategy. It should immediately reset the current
  // strategy because it's idle.
  UpdateStrategyWithCreationCounter(&allocator, &creation_count_b);

  // Next allocation should recreate the strategy (Strategy B).
  handle = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_EQ(creation_count_a, 1);
  EXPECT_EQ(creation_count_b, 1);

  allocator.Free(DemuxerStream::VIDEO, handle, 1024);
}

TEST(DecoderBufferAllocatorStrategyTest, SwitchStrategyPendingWhenBusy) {
  DecoderBufferAllocator allocator;
  const size_t alignment = allocator.GetBufferAlignment();

  int creation_count_a = 0;
  int creation_count_b = 0;

  // Start with Strategy A
  UpdateStrategyWithCreationCounter(&allocator, &creation_count_a);

  // Allocate to make it busy.
  auto handle_1 = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle_1, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_EQ(creation_count_a, 1);
  EXPECT_EQ(creation_count_b, 0);

  // Attempt to switch to Strategy B. It should be pending since allocator
  // is busy.
  UpdateStrategyWithCreationCounter(&allocator, &creation_count_b);

  auto handle_2 = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle_2, DecoderBuffer::Allocator::kInvalidHandle);
  // Still on Strategy A
  EXPECT_EQ(creation_count_a, 1);
  EXPECT_EQ(creation_count_b, 0);

  // Free the allocations. This should trigger the reset.
  allocator.Free(DemuxerStream::VIDEO, handle_1, 1024);
  allocator.Free(DemuxerStream::VIDEO, handle_2, 1024);

  // Strategy should have switched to Strategy B.
  // Next allocation should recreate a strategy.
  auto handle_3 = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle_3, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_EQ(creation_count_a, 1);
  EXPECT_EQ(creation_count_b, 1);

  allocator.Free(DemuxerStream::VIDEO, handle_3, 1024);
}

TEST(DecoderBufferAllocatorStrategyTest, RepeatedEnableDoesNothing) {
  DecoderBufferAllocator allocator;
  const size_t alignment = allocator.GetBufferAlignment();

  int creation_count_a = 0;
  int creation_count_b = 0;

  // 1. Start with Strategy A
  UpdateStrategyWithCreationCounter(&allocator, &creation_count_a);
  auto handle = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_EQ(creation_count_a, 1);
  EXPECT_EQ(creation_count_b, 0);

  // 2. Switch to Strategy B (pending because busy)
  UpdateStrategyWithCreationCounter(&allocator, &creation_count_b);
  EXPECT_EQ(creation_count_a, 1);
  EXPECT_EQ(creation_count_b, 0);

  // 3. Enable Strategy B again (repeated enable)
  UpdateStrategyWithCreationCounter(&allocator, &creation_count_b);

  // Freeing the allocations will trigger the pending reset.
  allocator.Free(DemuxerStream::VIDEO, handle, 1024);

  // Next allocation should recreate the strategy (Strategy B)
  handle = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle, DecoderBuffer::Allocator::kInvalidHandle);

  // creation_count_a remains 1, creation_count_b becomes 1
  EXPECT_EQ(creation_count_a, 1);
  EXPECT_EQ(creation_count_b, 1);

  allocator.Free(DemuxerStream::VIDEO, handle, 1024);
}

TEST(DecoderBufferAllocatorStrategyTest,
     FallbackToDefaultWhenStrategyCreateCBReturnsNull) {
  DecoderBufferAllocator allocator;
  const size_t alignment = allocator.GetBufferAlignment();

  // Inject a strategy callback that always returns nullptr.
  allocator.UpdateAllocatorStrategy(base::BindRepeating(
      [](int initial_capacity, int allocation_unit)
          -> std::unique_ptr<DecoderBufferAllocator::Strategy> {
        return nullptr;
      }));

  // This should not crash, it should log a warning and fallback to the
  // default strategy.
  auto handle = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle, DecoderBuffer::Allocator::kInvalidHandle);

  // The fallback strategies (ReuseAllocatorBase/InPlaceReuseAllocatorBase)
  // do not produce annotated pointers.
  EXPECT_GT(allocator.GetAllocatedMemory(), 0u);

  allocator.Free(DemuxerStream::VIDEO, handle, 1024);
}

TEST(DecoderBufferAllocatorStrategyTest,
     AllocateOnDemandRecreatesStrategyWhenAllocationsReachZero) {
  constexpr bool kIsAllocatedOnDemand = true;
  DecoderBufferAllocator allocator(kIsAllocatedOnDemand, 10 * 1024 * 1024,
                                   512 * 1024);
  const size_t alignment = allocator.GetBufferAlignment();

  int creation_count = 0;
  UpdateStrategyWithCreationCounter(&allocator, &creation_count);

  EXPECT_EQ(creation_count, 0);

  auto handle1 = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle1, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_EQ(creation_count, 1);

  allocator.Free(DemuxerStream::VIDEO, handle1, 1024);

  auto handle2 = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle2, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_EQ(creation_count, 2);

  allocator.Free(DemuxerStream::VIDEO, handle2, 1024);
}

TEST(DecoderBufferAllocatorStrategyTest,
     NoAllocateOnDemandKeepsStrategyWhenAllocationsReachZero) {
  constexpr bool kIsAllocatedOnDemand = false;
  DecoderBufferAllocator allocator(kIsAllocatedOnDemand, 10 * 1024 * 1024,
                                   512 * 1024);
  const size_t alignment = allocator.GetBufferAlignment();

  int creation_count = 0;
  UpdateStrategyWithCreationCounter(&allocator, &creation_count);

  auto handle1 = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle1, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_EQ(creation_count, 1);

  allocator.Free(DemuxerStream::VIDEO, handle1, 1024);

  auto handle2 = allocator.Allocate(DemuxerStream::VIDEO, 1024, alignment);
  ASSERT_NE(handle2, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_EQ(creation_count, 1);

  allocator.Free(DemuxerStream::VIDEO, handle2, 1024);
}

}  // namespace
}  // namespace media

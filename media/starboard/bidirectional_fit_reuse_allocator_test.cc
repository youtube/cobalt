/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/media/bidirectional_fit_reuse_allocator.h"

#include <memory>

#include "starboard/common/fixed_no_free_allocator.h"
#include "starboard/common/pointer_arithmetic.h"
#include "starboard/configuration.h"
#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct AlignedMemoryDeleter {
  void operator()(uint8_t* p) { free(p); }
};

class BidirectionalFitReuseAllocatorTest : public ::testing::Test {
 public:
  static const int kBufferSize = 1 * 1024 * 1024;

  BidirectionalFitReuseAllocatorTest() { ResetAllocator(); }

 protected:
  void ResetAllocator(std::size_t initial_capacity = 0,
                      std::size_t small_allocation_threshold = 0,
                      std::size_t allocation_increment = 0) {
    void* tmp = nullptr;
    posix_memalign(&tmp, starboard::common::Allocator::kMinAlignment,
                   kBufferSize);
    buffer_.reset(static_cast<uint8_t*>(tmp));

    std::unique_ptr<starboard::common::FixedNoFreeAllocator> fallback_allocator(
        new starboard::common::FixedNoFreeAllocator(buffer_.get(),
                                                    kBufferSize));
    allocator_.reset(new cobalt::media::BidirectionalFitReuseAllocator(
        fallback_allocator.get(), initial_capacity, small_allocation_threshold,
        allocation_increment));

    fallback_allocator_.swap(fallback_allocator);
  }

  std::unique_ptr<uint8_t, AlignedMemoryDeleter> buffer_;
  std::unique_ptr<starboard::common::FixedNoFreeAllocator> fallback_allocator_;
  std::unique_ptr<cobalt::media::BidirectionalFitReuseAllocator> allocator_;
};

}  // namespace

TEST_F(BidirectionalFitReuseAllocatorTest, AlignmentCheck) {
  const std::size_t kAlignments[] = {4, 16, 256, 32768};
  const std::size_t kBlockSizes[] = {4, 97, 256, 65201};
  for (int i = 0; i < SB_ARRAY_SIZE(kAlignments); ++i) {
    for (int j = 0; j < SB_ARRAY_SIZE(kBlockSizes); ++j) {
      void* p = allocator_->Allocate(kBlockSizes[j], kAlignments[i]);
      EXPECT_TRUE(p != NULL);
      EXPECT_EQ(starboard::common::IsAligned(p, kAlignments[i]), true);
      allocator_->Free(p);
    }
  }
}

// Check that the reuse allocator actually merges adjacent free blocks.
TEST_F(BidirectionalFitReuseAllocatorTest, FreeBlockMergingLeft) {
  const std::size_t kBlockSizes[] = {156, 16475};
  const std::size_t kAlignment = 4;
  void* blocks[] = {NULL, NULL};
  blocks[0] = allocator_->Allocate(kBlockSizes[0], kAlignment);
  blocks[1] = allocator_->Allocate(kBlockSizes[1], kAlignment);
  // In an empty allocator we expect first alloc to be < second.
  EXPECT_LT(reinterpret_cast<uintptr_t>(blocks[0]),
            reinterpret_cast<uintptr_t>(blocks[1]));
  allocator_->Free(blocks[0]);
  allocator_->Free(blocks[1]);
  // Should have merged blocks 1 with block 0.
  void* test_p =
      allocator_->Allocate(kBlockSizes[0] + kBlockSizes[1], kAlignment);
  EXPECT_EQ(test_p, blocks[0]);
  allocator_->Free(test_p);
}

TEST_F(BidirectionalFitReuseAllocatorTest, FreeBlockMergingRight) {
  const std::size_t kBlockSizes[] = {156, 202, 354};
  const std::size_t kAlignment = 4;
  void* blocks[] = {NULL, NULL, NULL};
  blocks[0] = allocator_->Allocate(kBlockSizes[0], kAlignment);
  blocks[1] = allocator_->Allocate(kBlockSizes[1], kAlignment);
  blocks[2] = allocator_->Allocate(kBlockSizes[2], kAlignment);
  // In an empty allocator we expect first alloc to be < second.
  EXPECT_LT(reinterpret_cast<uintptr_t>(blocks[1]),
            reinterpret_cast<uintptr_t>(blocks[2]));
  allocator_->Free(blocks[2]);
  allocator_->Free(blocks[1]);
  // Should have merged block 1 with block 2.
  void* test_p =
      allocator_->Allocate(kBlockSizes[1] + kBlockSizes[2], kAlignment);
  EXPECT_EQ(test_p, blocks[1]);
  allocator_->Free(test_p);
  allocator_->Free(blocks[0]);
}

TEST_F(BidirectionalFitReuseAllocatorTest, InitialCapacity) {
  const std::size_t kInitialCapacity = kBufferSize / 2;
  ResetAllocator(kInitialCapacity);
  EXPECT_GE(fallback_allocator_->GetAllocated(), kInitialCapacity);
}

TEST_F(BidirectionalFitReuseAllocatorTest, AllocationIncrement) {
  const std::size_t kAllocationIncrement = kBufferSize / 2;
  ResetAllocator(0, 0, kAllocationIncrement);
  void* p = allocator_->Allocate(1, 1);
  EXPECT_TRUE(p != NULL);
  allocator_->Free(p);
  EXPECT_GE(fallback_allocator_->GetAllocated(), kAllocationIncrement);
}

TEST_F(BidirectionalFitReuseAllocatorTest, FallbackBlockMerge) {
  void* p = allocator_->Allocate(kBufferSize, 1);
  EXPECT_TRUE(p != NULL);
  allocator_->Free(p);

  ResetAllocator();

  p = allocator_->Allocate(kBufferSize / 2, 1);
  EXPECT_TRUE(p != NULL);
  allocator_->Free(p);

  p = allocator_->Allocate(kBufferSize, 1);
  EXPECT_TRUE(p != NULL);
  allocator_->Free(p);
}

TEST_F(BidirectionalFitReuseAllocatorTest, AllocationsWithThreshold) {
  const std::size_t kSmallAllocationThreshold = 1024;

  ResetAllocator(kBufferSize, kSmallAllocationThreshold, 0);

  void* small_allocation_1 =
      allocator_->Allocate(kSmallAllocationThreshold - 1, 1);
  EXPECT_TRUE(small_allocation_1 != NULL);

  void* large_allocation_1 =
      allocator_->Allocate(kSmallAllocationThreshold + 1, 1);
  EXPECT_TRUE(large_allocation_1 != NULL);

  // According to the spec of BidirectionalFitReuseAllocator, any memory block
  // whose size is equal to the threshold is allocated from the back.
  void* small_allocation_2 = allocator_->Allocate(kSmallAllocationThreshold, 1);
  EXPECT_TRUE(small_allocation_2 != NULL);

  void* small_allocation_3 = allocator_->Allocate(1, 1);
  EXPECT_TRUE(small_allocation_3 != NULL);

  // Large allocations are allocated from the front, small allocations are
  // allocated from the back.
  EXPECT_LT(reinterpret_cast<uintptr_t>(large_allocation_1),
            reinterpret_cast<uintptr_t>(small_allocation_3));
  EXPECT_LT(reinterpret_cast<uintptr_t>(small_allocation_3),
            reinterpret_cast<uintptr_t>(small_allocation_2));
  EXPECT_LT(reinterpret_cast<uintptr_t>(small_allocation_2),
            reinterpret_cast<uintptr_t>(small_allocation_1));

  allocator_->Free(small_allocation_1);
  allocator_->Free(small_allocation_2);
  allocator_->Free(large_allocation_1);
}

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

#include "starboard/common/bidirectional_fit_reuse_allocator.h"

#include <memory>

#include "starboard/common/fixed_no_free_allocator.h"
#include "starboard/common/in_place_reuse_allocator_base.h"
#include "starboard/common/pointer_arithmetic.h"
#include "starboard/common/reuse_allocator_base.h"
#include "starboard/configuration.h"
#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace common {

struct AlignedMemoryDeleter {
  void operator()(uint8_t* p) { free(p); }
};

static constexpr int kBufferSize = 1 * 1024 * 1024;

template <typename ReuseAllocatorBase>
class BidirectionalFitReuseAllocatorTest : public ::testing::Test {
 public:
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
    allocator_.reset(new BidirectionalFitReuseAllocator<ReuseAllocatorBase>(
        fallback_allocator.get(), initial_capacity, small_allocation_threshold,
        allocation_increment));

    fallback_allocator_.swap(fallback_allocator);
  }

  std::unique_ptr<uint8_t, AlignedMemoryDeleter> buffer_;
  std::unique_ptr<starboard::common::FixedNoFreeAllocator> fallback_allocator_;
  std::unique_ptr<BidirectionalFitReuseAllocator<ReuseAllocatorBase>>
      allocator_;
};

typedef ::testing::Types<starboard::common::InPlaceReuseAllocatorBase,
                         starboard::common::ReuseAllocatorBase>
    Implementations;

class ClassNameGenerator {
 public:
  template <typename T>
  static std::string GetName(int) {
    if constexpr (std::is_same_v<
                      T, starboard::common::InPlaceReuseAllocatorBase>) {
      return "InPlaceReuseAllocatorBase";
    }
    if constexpr (std::is_same_v<T, starboard::common::ReuseAllocatorBase>) {
      return "ReuseAllocatorBase";
    }
  }
};

TYPED_TEST_SUITE(BidirectionalFitReuseAllocatorTest,
                 Implementations,
                 ClassNameGenerator);

TYPED_TEST(BidirectionalFitReuseAllocatorTest, SunnyDay) {
  const std::size_t kAlignment = sizeof(void*);
  const std::size_t kBlockSizes[] = {4, 97, 256, 65201};

  for (int j = 0; j < SB_ARRAY_SIZE(kBlockSizes); ++j) {
    void* p = this->allocator_->Allocate(kBlockSizes[j], kAlignment);
    EXPECT_TRUE(p != NULL);
    EXPECT_EQ(starboard::common::IsAligned(p, kAlignment), true);
    this->allocator_->Free(p);
  }
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, FreeFromFront) {
  const std::size_t kAlignment = sizeof(void*);

  for (int loop = 0; loop < 2; ++loop) {
    void* p[3];
    for (int i = 0; i < sizeof(p) / sizeof(*p); ++i) {
      p[i] = this->allocator_->Allocate(128, kAlignment);
      EXPECT_TRUE(p[i] != NULL);
    }

    for (int i = 0; i < sizeof(p) / sizeof(*p); ++i) {
      this->allocator_->Free(p[i]);
    }
  }
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, FreeFromBack) {
  const std::size_t kAlignment = sizeof(void*);

  for (int loop = 0; loop < 2; ++loop) {
    void* p[3];
    for (int i = 0; i < sizeof(p) / sizeof(*p); ++i) {
      p[i] = this->allocator_->Allocate(128, kAlignment);
      EXPECT_TRUE(p[i] != NULL);
    }

    // free in reverse order
    for (int i = sizeof(p) / sizeof(*p) - 1; i >= 0; --i) {
      this->allocator_->Free(p[i]);
    }
  }
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, FreeFromMiddle) {
  const std::size_t kAlignment = sizeof(void*);

  for (int loop = 0; loop < 2; ++loop) {
    void* p[3];
    for (int i = 0; i < sizeof(p) / sizeof(*p); ++i) {
      p[i] = this->allocator_->Allocate(128, kAlignment);
      EXPECT_TRUE(p[i] != NULL);
    }

    this->allocator_->Free(p[1]);  // middle first
    this->allocator_->Free(p[2]);
    this->allocator_->Free(p[0]);
  }
}

// Check that the reuse allocator actually merges adjacent free blocks.
TYPED_TEST(BidirectionalFitReuseAllocatorTest, FreeBlockMergingLeft) {
  const std::size_t kBlockSizes[] = {156, 16475};
  const std::size_t kAlignment = sizeof(void*);
  void* blocks[] = {NULL, NULL};
  blocks[0] = this->allocator_->Allocate(kBlockSizes[0], kAlignment);
  blocks[1] = this->allocator_->Allocate(kBlockSizes[1], kAlignment);
  // In an empty allocator we expect first alloc to be < second.
  EXPECT_LT(reinterpret_cast<uintptr_t>(blocks[0]),
            reinterpret_cast<uintptr_t>(blocks[1]));
  this->allocator_->Free(blocks[0]);
  this->allocator_->Free(blocks[1]);
  // Should have merged blocks 1 with block 0.
  void* test_p =
      this->allocator_->Allocate(kBlockSizes[0] + kBlockSizes[1], kAlignment);
  EXPECT_EQ(test_p, blocks[0]);
  this->allocator_->Free(test_p);
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, FreeBlockMergingRight) {
  const std::size_t kBlockSizes[] = {156, 202, 354};
  const std::size_t kAlignment = sizeof(void*);
  void* blocks[] = {NULL, NULL, NULL};
  blocks[0] = this->allocator_->Allocate(kBlockSizes[0], kAlignment);
  blocks[1] = this->allocator_->Allocate(kBlockSizes[1], kAlignment);
  blocks[2] = this->allocator_->Allocate(kBlockSizes[2], kAlignment);
  // In an empty allocator we expect first alloc to be < second.
  EXPECT_LT(reinterpret_cast<uintptr_t>(blocks[1]),
            reinterpret_cast<uintptr_t>(blocks[2]));
  this->allocator_->Free(blocks[2]);
  this->allocator_->Free(blocks[1]);
  // Should have merged block 1 with block 2.
  void* test_p =
      this->allocator_->Allocate(kBlockSizes[1] + kBlockSizes[2], kAlignment);
  EXPECT_EQ(test_p, blocks[1]);
  this->allocator_->Free(test_p);
  this->allocator_->Free(blocks[0]);
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, InitialCapacity) {
  const std::size_t kInitialCapacity = kBufferSize / 2;
  this->ResetAllocator(kInitialCapacity);
  EXPECT_GE(this->fallback_allocator_->GetAllocated(), kInitialCapacity);
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, AllocationIncrement) {
  const std::size_t kAllocationIncrement = kBufferSize / 2;
  const std::size_t kAlignment = sizeof(void*);
  this->ResetAllocator(0, 0, kAllocationIncrement);
  void* p = this->allocator_->Allocate(1, kAlignment);
  EXPECT_TRUE(p != NULL);
  this->allocator_->Free(p);
  EXPECT_GE(this->fallback_allocator_->GetAllocated(), kAllocationIncrement);
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, FallbackBlockMerge) {
  const std::size_t kAlignment = sizeof(void*);
  void* p = this->allocator_->Allocate(kBufferSize / 2, kAlignment);
  EXPECT_TRUE(p != NULL);
  this->allocator_->Free(p);

  this->ResetAllocator();

  p = this->allocator_->Allocate(kBufferSize / 2, kAlignment);
  EXPECT_TRUE(p != NULL);
  this->allocator_->Free(p);

  p = this->allocator_->Allocate(kBufferSize / 4 * 3, kAlignment);
  EXPECT_TRUE(p != NULL);
  this->allocator_->Free(p);
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, AllocationsWithThreshold) {
  const std::size_t kSmallAllocationThreshold = 1024;
  const std::size_t kAlignment = sizeof(void*);

  this->ResetAllocator(kBufferSize, kSmallAllocationThreshold, 0);

  void* small_allocation_1 =
      this->allocator_->Allocate(kSmallAllocationThreshold - 256, kAlignment);
  EXPECT_TRUE(small_allocation_1 != NULL);

  void* large_allocation_1 =
      this->allocator_->Allocate(kSmallAllocationThreshold + 1, kAlignment);
  EXPECT_TRUE(large_allocation_1 != NULL);

  void* small_allocation_2 =
      this->allocator_->Allocate(kSmallAllocationThreshold - 128, kAlignment);
  EXPECT_TRUE(small_allocation_2 != NULL);

  void* small_allocation_3 = this->allocator_->Allocate(1, kAlignment);
  EXPECT_TRUE(small_allocation_3 != NULL);

  // Large allocations are allocated from the front, small allocations are
  // allocated from the back.
  EXPECT_LT(reinterpret_cast<uintptr_t>(large_allocation_1),
            reinterpret_cast<uintptr_t>(small_allocation_3));
  EXPECT_LT(reinterpret_cast<uintptr_t>(small_allocation_3),
            reinterpret_cast<uintptr_t>(small_allocation_2));
  EXPECT_LT(reinterpret_cast<uintptr_t>(small_allocation_2),
            reinterpret_cast<uintptr_t>(small_allocation_1));

  this->allocator_->Free(small_allocation_2);
  this->allocator_->Free(large_allocation_1);
  this->allocator_->Free(small_allocation_1);
  this->allocator_->Free(small_allocation_3);
}

}  // namespace common
}  // namespace starboard

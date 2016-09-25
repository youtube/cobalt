/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "nb/reuse_allocator.h"

#include "nb/fixed_no_free_allocator.h"
#include "nb/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

inline bool IsAligned(void* ptr, std::size_t boundary) {
  uintptr_t ptr_as_int = reinterpret_cast<uintptr_t>(ptr);
  return ptr_as_int % boundary == 0;
}

class ReuseAllocatorTest : public ::testing::Test {
 public:
  static const int kBufferSize = 1 * 1024 * 1024;

  ReuseAllocatorTest() { ResetAllocator(0); }

 protected:
  void ResetAllocator(size_t small_allocation_threshold) {
    buffer_.reset(new uint8_t[kBufferSize]);
    fallback_allocator_.reset(
        new nb::FixedNoFreeAllocator(buffer_.get(), kBufferSize));
    if (small_allocation_threshold == 0) {
      allocator_.reset(new nb::ReuseAllocator(fallback_allocator_.get()));
    } else {
      allocator_.reset(new nb::ReuseAllocator(
          fallback_allocator_.get(), kBufferSize, small_allocation_threshold));
    }
  }

  nb::scoped_array<uint8_t> buffer_;
  nb::scoped_ptr<nb::FixedNoFreeAllocator> fallback_allocator_;
  nb::scoped_ptr<nb::ReuseAllocator> allocator_;
};

}  // namespace

TEST_F(ReuseAllocatorTest, AlignmentCheck) {
  const std::size_t kAlignments[] = {4, 16, 256, 32768};
  const std::size_t kBlockSizes[] = {4, 97, 256, 65201};
  for (int i = 0; i < SB_ARRAY_SIZE(kAlignments); ++i) {
    for (int j = 0; j < SB_ARRAY_SIZE(kBlockSizes); ++j) {
      void* p = allocator_->Allocate(kBlockSizes[j], kAlignments[i]);
      // NOTE: Don't dereference p- this doesn't point anywhere valid.
      EXPECT_TRUE(p != NULL);
      EXPECT_EQ(IsAligned(p, kAlignments[i]), true);
      allocator_->Free(p);
    }
  }
}

// Check that the reuse allocator actually merges adjacent free blocks.
TEST_F(ReuseAllocatorTest, FreeBlockMergingLeft) {
  const std::size_t kBlockSizes[] = {156, 202};
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

TEST_F(ReuseAllocatorTest, FreeBlockMergingRight) {
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

TEST_F(ReuseAllocatorTest, SmallAlloc) {
  // Recreate allocator with small allocation threshold to 256.
  ResetAllocator(256);

  const std::size_t kBlockSizes[] = {117, 193, 509, 1111};
  const std::size_t kAlignment = 16;
  void* blocks[] = {NULL, NULL, NULL, NULL};
  for (int i = 0; i < SB_ARRAY_SIZE(kBlockSizes); ++i) {
    blocks[i] = allocator_->Allocate(kBlockSizes[i], kAlignment);
  }
  // The two small allocs should be in the back in reverse order.
  EXPECT_GT(reinterpret_cast<uintptr_t>(blocks[0]),
            reinterpret_cast<uintptr_t>(blocks[1]));
  // Small allocs should has higher address than other allocs.
  EXPECT_GT(reinterpret_cast<uintptr_t>(blocks[1]),
            reinterpret_cast<uintptr_t>(blocks[3]));
  // Non-small allocs are allocated from the front and the first one has the
  // lowest address.
  EXPECT_LT(reinterpret_cast<uintptr_t>(blocks[2]),
            reinterpret_cast<uintptr_t>(blocks[3]));
  for (int i = 0; i < SB_ARRAY_SIZE(kBlockSizes); ++i) {
    allocator_->Free(blocks[i]);
  }
  // Should have one single free block equals to the capacity.
  void* test_p = allocator_->Allocate(allocator_->GetCapacity());
  EXPECT_TRUE(test_p != NULL);
  allocator_->Free(test_p);
}

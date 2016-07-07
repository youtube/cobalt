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

#include "base/memory/scoped_ptr.h"
#include "cobalt/base/fixed_no_free_allocator.h"
#include "cobalt/base/reuse_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

inline bool IsAligned(void* ptr, size_t boundary) {
  uintptr_t ptr_as_int = reinterpret_cast<uintptr_t>(ptr);
  return ptr_as_int % boundary == 0;
}

class ReuseAllocatorTest : public ::testing::Test {
 public:
  static const int kBufferSize = 1 * 1024 * 1024;

  ReuseAllocatorTest() {
    buffer_.reset(new uint8_t[kBufferSize]);
    fallback_allocator_.reset(
        new base::FixedNoFreeAllocator(buffer_.get(), kBufferSize));
    allocator_.reset(new base::ReuseAllocator(fallback_allocator_.get()));
  }

 protected:
  scoped_array<uint8_t> buffer_;
  scoped_ptr<base::FixedNoFreeAllocator> fallback_allocator_;
  scoped_ptr<base::ReuseAllocator> allocator_;
};

}  // namespace

TEST_F(ReuseAllocatorTest, AlignmentCheck) {
  const size_t kAlignments[] = {4, 16, 256, 32768};
  const size_t kBlockSizes[] = {4, 97, 256, 65201};
  for (int i = 0; i < arraysize(kAlignments); ++i) {
    for (int j = 0; j < arraysize(kBlockSizes); ++j) {
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
  const size_t kBlockSizes[] = {156, 202};
  const size_t kAlignment = 4;
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
  const size_t kBlockSizes[] = {156, 202, 354};
  const size_t kAlignment = 4;
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

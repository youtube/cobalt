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

#include <cstddef>
#include <cstdint>
#include <memory>

#include "starboard/common/embedded_metadata_reuse_allocator_base.h"
#include "starboard/common/external_metadata_reuse_allocator_base.h"
#include "starboard/common/fixed_no_free_allocator.h"
#include "starboard/common/pointer_arithmetic.h"
#include "starboard/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {

struct AlignedMemoryDeleter {
  void operator()(uint8_t* p) { free(p); }
};

static constexpr int kBufferSize = 8 * 1024 * 1024;

template <typename ReuseAllocatorBase>
class BidirectionalFitReuseAllocatorTest : public ::testing::Test {
 public:
  BidirectionalFitReuseAllocatorTest() { ResetAllocator(); }

 protected:
  class MockDecommitAllocator : public FixedNoFreeAllocator {
   public:
    MockDecommitAllocator(void* memory_start, size_t memory_size)
        : FixedNoFreeAllocator(memory_start, memory_size), decommit_count(0) {}

    void* Allocate(size_t size) override {
      void* ptr = FixedNoFreeAllocator::Allocate(size);
      if (ptr) {
        active_allocations_[ptr] = size;
      }
      return ptr;
    }

    void* Allocate(size_t size, size_t alignment) override {
      void* ptr = FixedNoFreeAllocator::Allocate(size, alignment);
      if (ptr) {
        active_allocations_[ptr] = size;
      }
      return ptr;
    }

    void* AllocateForAlignment(size_t* size, size_t alignment) override {
      void* ptr = FixedNoFreeAllocator::AllocateForAlignment(size, alignment);
      if (ptr) {
        active_allocations_[ptr] = *size;
      }
      return ptr;
    }

    void Free(void* memory) override {
      auto it = active_allocations_.find(memory);
      EXPECT_NE(it, active_allocations_.end())
          << "Freeing a pointer not allocated by this fallback allocator, or "
             "already freed.";
      if (it != active_allocations_.end()) {
        active_allocations_.erase(it);
      }
      FixedNoFreeAllocator::Free(memory);
    }

    void FreeWithSize(void* memory, size_t size) override {
      auto it = active_allocations_.find(memory);
      EXPECT_NE(it, active_allocations_.end())
          << "Freeing a pointer not allocated by this fallback allocator, or "
             "already freed.";
      if (it != active_allocations_.end()) {
        EXPECT_EQ(it->second, size)
            << "Free size does not exactly match the allocated size.";
        active_allocations_.erase(it);
      }
      FixedNoFreeAllocator::FreeWithSize(memory, size);
    }

    void Decommit(void* memory, size_t size, bool conservative) override {
      auto it = active_allocations_.find(memory);
      EXPECT_NE(it, active_allocations_.end())
          << "Decommitting a pointer not allocated by this fallback allocator.";
      if (it != active_allocations_.end()) {
        EXPECT_EQ(it->second, size)
            << "Decommit size does not exactly match the allocated size.";
      }

      decommits.push_back({memory, size, conservative});
      decommit_count++;
    }

    struct DecommitRecord {
      void* memory;
      size_t size;
      bool conservative;
    };

    int decommit_count;
    std::vector<DecommitRecord> decommits;
    std::map<void*, size_t> active_allocations_;
  };

  void ResetAllocator(size_t initial_capacity = 0,
                      size_t small_allocation_threshold = 0,
                      size_t allocation_increment = 0) {
    void* tmp = nullptr;
    std::ignore = posix_memalign(&tmp, Allocator::kMinAlignment, kBufferSize);
    buffer_.reset(static_cast<uint8_t*>(tmp));

    std::unique_ptr<FixedNoFreeAllocator> fallback_allocator(
        new FixedNoFreeAllocator(buffer_.get(), kBufferSize));
    allocator_.reset(new BidirectionalFitReuseAllocator<ReuseAllocatorBase>(
        fallback_allocator.get(), initial_capacity, small_allocation_threshold,
        allocation_increment));

    fallback_allocator_.swap(fallback_allocator);
  }
  void ResetAllocatorWithDecommitSupport() {
    void* tmp = nullptr;
    std::ignore = posix_memalign(&tmp, Allocator::kMinAlignment, kBufferSize);
    buffer_.reset(static_cast<uint8_t*>(tmp));

    std::unique_ptr<MockDecommitAllocator> fallback_allocator(
        new MockDecommitAllocator(buffer_.get(), kBufferSize));

    // Store pointer before move/swap
    mock_fallback_allocator_ = fallback_allocator.get();

    allocator_.reset(new BidirectionalFitReuseAllocator<ReuseAllocatorBase>(
        mock_fallback_allocator_, 0, 0, 0, true,
        /*retain_blocks=*/0,
        /*conservative_decommit_blocks=*/0));

    fallback_allocator_.reset(fallback_allocator.release());
  }

  void ResetAllocatorWithTieredDecommitSupport(
      size_t retain_blocks,
      size_t conservative_decommit_blocks) {
    void* tmp = nullptr;
    std::ignore = posix_memalign(&tmp, Allocator::kMinAlignment, kBufferSize);
    buffer_.reset(static_cast<uint8_t*>(tmp));

    std::unique_ptr<MockDecommitAllocator> fallback_allocator(
        new MockDecommitAllocator(buffer_.get(), kBufferSize));

    // Store pointer before move/swap
    mock_fallback_allocator_ = fallback_allocator.get();

    allocator_.reset(new BidirectionalFitReuseAllocator<ReuseAllocatorBase>(
        mock_fallback_allocator_, 0, 0, 0, true, retain_blocks,
        conservative_decommit_blocks));

    fallback_allocator_.reset(fallback_allocator.release());
  }

  std::unique_ptr<uint8_t, AlignedMemoryDeleter> buffer_;
  std::unique_ptr<FixedNoFreeAllocator> fallback_allocator_;
  MockDecommitAllocator* mock_fallback_allocator_ = nullptr;
  std::unique_ptr<BidirectionalFitReuseAllocator<ReuseAllocatorBase>>
      allocator_;
};

typedef ::testing::Types<starboard::EmbeddedMetadataReuseAllocatorBase,
                         starboard::ExternalMetadataReuseAllocatorBase>
    Implementations;

class ClassNameGenerator {
 public:
  template <typename T>
  static std::string GetName(int) {
    if constexpr (std::is_same_v<
                      T, starboard::EmbeddedMetadataReuseAllocatorBase>) {
      return "EmbeddedMetadataReuseAllocatorBase";
    }
    if constexpr (std::is_same_v<
                      T, starboard::ExternalMetadataReuseAllocatorBase>) {
      return "ExternalMetadataReuseAllocatorBase";
    }
  }
};

TYPED_TEST_SUITE(BidirectionalFitReuseAllocatorTest,
                 Implementations,
                 ClassNameGenerator);

TYPED_TEST(BidirectionalFitReuseAllocatorTest, SunnyDay) {
  const size_t kAlignment = sizeof(void*);
  const size_t kBlockSizes[] = {4, 97, 256, 65201};

  for (size_t j = 0; j < SB_ARRAY_SIZE(kBlockSizes); ++j) {
    void* p = this->allocator_->Allocate(kBlockSizes[j], kAlignment);
    EXPECT_TRUE(p != NULL);
    EXPECT_EQ(starboard::IsAligned(p, kAlignment), true);
    this->allocator_->Free(p);
  }
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, FreeFromFront) {
  const size_t kAlignment = sizeof(void*);

  for (int loop = 0; loop < 2; ++loop) {
    void* p[3];
    for (size_t i = 0; i < sizeof(p) / sizeof(*p); ++i) {
      p[i] = this->allocator_->Allocate(128, kAlignment);
      EXPECT_TRUE(p[i] != NULL);
    }

    for (size_t i = 0; i < sizeof(p) / sizeof(*p); ++i) {
      this->allocator_->Free(p[i]);
    }
  }
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, FreeFromBack) {
  const size_t kAlignment = sizeof(void*);

  for (int loop = 0; loop < 2; ++loop) {
    void* p[3];
    for (size_t i = 0; i < sizeof(p) / sizeof(*p); ++i) {
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
  const size_t kAlignment = sizeof(void*);

  for (int loop = 0; loop < 2; ++loop) {
    void* p[3];
    for (size_t i = 0; i < sizeof(p) / sizeof(*p); ++i) {
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
  const size_t kBlockSizes[] = {156, 16475};
  const size_t kAlignment = sizeof(void*);
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
  const size_t kBlockSizes[] = {156, 202, 354};
  const size_t kAlignment = sizeof(void*);
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
  const size_t kInitialCapacity = kBufferSize / 2;
  this->ResetAllocator(kInitialCapacity);
  EXPECT_GE(this->fallback_allocator_->GetAllocated(), kInitialCapacity);
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, AllocationIncrement) {
  const size_t kAllocationIncrement = kBufferSize / 2;
  const size_t kAlignment = sizeof(void*);
  this->ResetAllocator(0, 0, kAllocationIncrement);
  void* p = this->allocator_->Allocate(1, kAlignment);
  EXPECT_TRUE(p != NULL);
  this->allocator_->Free(p);
  EXPECT_GE(this->fallback_allocator_->GetAllocated(), kAllocationIncrement);
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, FallbackBlockMerge) {
  const size_t kAlignment = sizeof(void*);
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
  const size_t kSmallAllocationThreshold = 1024;
  const size_t kAlignment = sizeof(void*);

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

TYPED_TEST(BidirectionalFitReuseAllocatorTest, DecommitExactFallbackBlocks) {
  const size_t kAlignment = sizeof(void*);

  this->ResetAllocatorWithDecommitSupport();

  // Make two allocations. Because the initial capacity is 0, each allocate call
  // will force the reuse allocator to expand and grab a distinct block from the
  // fallback allocator. We use sizes large enough (multiples of 64KB) to ensure
  // that even if the fallback allocator aligns the allocated size up to a page
  // boundary, the second allocation will not fit in any leftover space.
  const size_t size1 = 64 * 1024;
  const size_t size2 = 128 * 1024;
  void* block1 = this->allocator_->Allocate(size1, kAlignment);
  void* block2 = this->allocator_->Allocate(size2, kAlignment);

  EXPECT_TRUE(block1 != nullptr);
  EXPECT_TRUE(block2 != nullptr);
  EXPECT_EQ(this->mock_fallback_allocator_->decommits.size(), 0);

  // Free them. The allocator may merge the free blocks internally,
  // but it must still decommit the original exact fallback blocks.
  this->allocator_->Free(block1);
  this->allocator_->Free(block2);
  this->allocator_->DecommitAllDecommitableBlocks();

  // We should have exactly 2 decommits matching the original blocks.
  EXPECT_EQ(this->mock_fallback_allocator_->decommits.size(), 2);

  // They might be decommitted in any order, so check if both exist.
  // Note: For EmbeddedMetadataReuseAllocatorBase, `block1` and `block2` are
  // offset from the raw fallback memory pointer by `sizeof(BlockMetadata)`.
  // Thus, we check if the original fallback block `contains` the user pointer,
  // rather than checking for exact pointer equality.
  bool found_block1 = false;
  bool found_block2 = false;
  for (const auto& decommit : this->mock_fallback_allocator_->decommits) {
    uint8_t* fallback_ptr = static_cast<uint8_t*>(decommit.memory);
    size_t fallback_size = decommit.size;
    uint8_t* ptr1 = static_cast<uint8_t*>(block1);
    uint8_t* ptr2 = static_cast<uint8_t*>(block2);

    if (ptr1 >= fallback_ptr && ptr1 < fallback_ptr + fallback_size &&
        fallback_size >= size1) {
      found_block1 = true;
    }
    if (ptr2 >= fallback_ptr && ptr2 < fallback_ptr + fallback_size &&
        fallback_size >= size2) {
      found_block2 = true;
    }
  }

  EXPECT_TRUE(found_block1);
  EXPECT_TRUE(found_block2);
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, DecommitOnBatchedFree) {
  const size_t kAlignment = sizeof(void*);
  const size_t kBlockSize = 1024;
  const int kNumBlocks = 4096;

  this->ResetAllocatorWithDecommitSupport();

  std::vector<void*> blocks;
  // Allocate a large number of blocks to trigger the batched free threshold
  // (the class currently requires > 32).
  for (int i = 0; i < kNumBlocks; ++i) {
    void* block = this->allocator_->Allocate(kBlockSize, kAlignment);
    EXPECT_TRUE(block != nullptr);
    blocks.push_back(block);
  }

  EXPECT_EQ(this->mock_fallback_allocator_->decommits.size(), 0);

  for (void* block : blocks) {
    this->allocator_->Free(block);
  }
  this->allocator_->DecommitAllDecommitableBlocks();

  // EmbeddedMetadataReuseAllocatorBase batches frees and processes them all at
  // once when idle.
  EXPECT_GT(this->mock_fallback_allocator_->decommits.size(), 0);

  size_t total_decommitted_size = 0;
  for (const auto& decommit : this->mock_fallback_allocator_->decommits) {
    total_decommitted_size += decommit.size;
    EXPECT_FALSE(decommit.conservative);
  }
  EXPECT_GE(total_decommitted_size, kBlockSize * kNumBlocks);
}

TYPED_TEST(BidirectionalFitReuseAllocatorTest, TieredDecommit) {
  const size_t kAlignment = sizeof(void*);

  // In this test, each allocate call will allocate a block of size 64KB (+
  // metadata for InPlace). Since allocation_increment is 0, each will trigger a
  // fallback allocation. So we pass blocks directly: 1 block retained, 2 blocks
  // conservatively decommitted.
  this->ResetAllocatorWithTieredDecommitSupport(/*retain_blocks=*/1,
                                                /*conservative_blocks=*/2);

  // Allocate 4 blocks of 64KB each.
  // Block 1 (0 - 64KB) fits entirely in the retain window.
  // Block 2 (64KB - 128KB) falls in the conservative window.
  // Block 3 (128KB - 192KB) falls in the conservative window (starts at 128KB,
  // bytes_past_retain=64KB < 128KB). Block 4 (192KB - 256KB) falls in the
  // aggressive window (starts at 192KB, bytes_past_retain=128KB >= 128KB).
  const size_t kBlockSize = 64 * 1024;
  void* block1 = this->allocator_->Allocate(kBlockSize, kAlignment);
  void* block2 = this->allocator_->Allocate(kBlockSize, kAlignment);
  void* block3 = this->allocator_->Allocate(kBlockSize, kAlignment);
  void* block4 = this->allocator_->Allocate(kBlockSize, kAlignment);

  EXPECT_TRUE(block1 != nullptr);
  EXPECT_TRUE(block2 != nullptr);
  EXPECT_TRUE(block3 != nullptr);
  EXPECT_TRUE(block4 != nullptr);
  EXPECT_EQ(this->mock_fallback_allocator_->decommits.size(), 0);

  // Free them. The allocator should batch them and process when idle.
  this->allocator_->Free(block1);
  this->allocator_->Free(block2);
  this->allocator_->Free(block3);
  this->allocator_->Free(block4);
  this->allocator_->DecommitAllDecommitableBlocks();

  // Block 1 (retain) is skipped. Block 2, 3, 4 should be decommitted.
  EXPECT_EQ(this->mock_fallback_allocator_->decommits.size(), 3);

  bool found_block2 = false;
  bool found_block3 = false;
  bool found_block4 = false;
  for (const auto& decommit : this->mock_fallback_allocator_->decommits) {
    uint8_t* fallback_ptr = static_cast<uint8_t*>(decommit.memory);
    size_t fallback_size = decommit.size;
    uint8_t* ptr2 = static_cast<uint8_t*>(block2);
    uint8_t* ptr3 = static_cast<uint8_t*>(block3);
    uint8_t* ptr4 = static_cast<uint8_t*>(block4);

    if (ptr2 >= fallback_ptr && ptr2 < fallback_ptr + fallback_size) {
      found_block2 = true;
      EXPECT_TRUE(decommit.conservative);
    }
    if (ptr3 >= fallback_ptr && ptr3 < fallback_ptr + fallback_size) {
      found_block3 = true;
      EXPECT_TRUE(decommit.conservative);
    }
    if (ptr4 >= fallback_ptr && ptr4 < fallback_ptr + fallback_size) {
      found_block4 = true;
      EXPECT_FALSE(decommit.conservative);
    }
  }

  EXPECT_TRUE(found_block2);
  EXPECT_TRUE(found_block3);
  EXPECT_TRUE(found_block4);
}

}  // namespace starboard

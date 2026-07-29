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

#include "starboard/shared/starboard/player/pooled_allocator.h"

#include <thread>
#include <variant>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {
using FixedBlockPool = internal::FixedBlockPool;

// Helpers for FixedBlockPool tests
void* AllocateOrDie(FixedBlockPool& pool) {
  void* ptr = pool.TryAllocate();
  if (ptr) {
    return ptr;
  }
  ADD_FAILURE() << "Allocation failed when success was expected.";
  return nullptr;
}

// ============================================================================
// FixedBlockPool Tests
// ============================================================================

TEST(FixedBlockPoolTest, BasicAllocationAndFree) {
  const size_t kBlockSize = 32;
  const size_t kTotalBlocks = 4;
  FixedBlockPool pool("TestPool", kBlockSize, kTotalBlocks);

  EXPECT_EQ(pool.GetInfoForTesting().block_size, kBlockSize);
  EXPECT_EQ(pool.GetInfoForTesting().total_blocks, kTotalBlocks);
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, kTotalBlocks);

  std::vector<void*> allocated_blocks;

  // 1. Allocate all blocks.
  for (size_t i = 0; i < kTotalBlocks; ++i) {
    void* ptr = AllocateOrDie(pool);
    ASSERT_NE(ptr, nullptr);
    allocated_blocks.push_back(ptr);
  }
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 0U);

  // 2. Try to allocate one more. Should return nullptr (exhausted).
  EXPECT_EQ(pool.TryAllocate(), nullptr);
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 0U);

  // 3. Freeing nullptr should return false.
  EXPECT_FALSE(pool.TryFree(nullptr));

  // 4. Free all pool blocks. Should return true for all.
  for (void* ptr : allocated_blocks) {
    EXPECT_TRUE(pool.TryFree(ptr));
  }
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, kTotalBlocks);

  // 5. Allocate again. Should recycle.
  void* ptr = AllocateOrDie(pool);
  EXPECT_NE(ptr, nullptr);
  EXPECT_TRUE(pool.TryFree(ptr));
}

TEST(FixedBlockPoolTest, FreeRouting) {
  const size_t kBlockSize =
      24;  // Use a non-power-of-two to test arbitrary block sizes
  const size_t kTotalBlocks = 2;
  FixedBlockPool pool("TestPool", kBlockSize, kTotalBlocks);

  void* ptr1 = AllocateOrDie(pool);
  void* ptr2 = AllocateOrDie(pool);

  ASSERT_NE(ptr1, nullptr);
  ASSERT_NE(ptr2, nullptr);

  // Null pointer.
  EXPECT_FALSE(pool.TryFree(nullptr));

  // Heap pointer.
  int* heap_ptr = new int;
  EXPECT_FALSE(pool.TryFree(heap_ptr));
  delete heap_ptr;

  // Pointer past the end of the pool.
  uint8_t* past_end_ptr =
      static_cast<uint8_t*>(ptr1) + kBlockSize * kTotalBlocks;
  EXPECT_FALSE(pool.TryFree(past_end_ptr));

  // Valid pool pointers.
  EXPECT_TRUE(pool.TryFree(ptr1));
  EXPECT_TRUE(pool.TryFree(ptr2));
}

TEST(FixedBlockPoolTest, NonPowerOfTwoBlockSize) {
  const size_t kBlockSize = 27;  // Non-power-of-two!
  const size_t kTotalBlocks = 5;
  FixedBlockPool pool("TestPool", kBlockSize, kTotalBlocks);

  std::vector<void*> allocated;
  for (size_t i = 0; i < kTotalBlocks; ++i) {
    void* ptr = AllocateOrDie(pool);
    ASSERT_NE(ptr, nullptr);
    allocated.push_back(ptr);
  }

  // Free them all. If IsAligned (modulo) works, this will NOT crash!
  for (void* ptr : allocated) {
    EXPECT_TRUE(pool.TryFree(ptr));
  }
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, kTotalBlocks);
}

TEST(FixedBlockPoolTest, ThreadSafety) {
  const size_t kBlockSize = 64;
  const size_t kTotalBlocks = 10;
  FixedBlockPool pool("TestPool", kBlockSize, kTotalBlocks);

  const int kNumThreads = 4;
  const int kIterations = 100;

  std::vector<std::thread> threads;
  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&pool]() {
      for (int i = 0; i < kIterations; ++i) {
        std::vector<void*> allocated;
        // Allocate up to 2 blocks per thread.
        for (size_t j = 0; j < 2; ++j) {
          void* ptr = AllocateOrDie(pool);
          ASSERT_NE(ptr, nullptr);
          allocated.push_back(ptr);
        }
        std::this_thread::yield();
        for (void* ptr : allocated) {
          EXPECT_TRUE(pool.TryFree(ptr));
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, kTotalBlocks);
}

using FixedBlockPoolDeathTest = ::testing::Test;

TEST_F(FixedBlockPoolDeathTest, DeathTest_InvalidConstructorArgs) {
  EXPECT_DEATH_IF_SUPPORTED(FixedBlockPool("TestPool", 0, 10), "");
  EXPECT_DEATH_IF_SUPPORTED(FixedBlockPool("TestPool", 10, 0), "");
}

TEST_F(FixedBlockPoolDeathTest, DeathTest_FreeMisalignedPoolPointer) {
  FixedBlockPool pool("TestPool", 16, 2);
  void* ptr = AllocateOrDie(pool);
  void* misaligned = static_cast<uint8_t*>(ptr) + 1;
  EXPECT_DEATH_IF_SUPPORTED(pool.TryFree(misaligned), "");
  pool.TryFree(ptr);  // Cleanup
}

// ============================================================================
// PooledAllocator Tests
// ============================================================================

TEST(PooledAllocatorTest, SinglePoolFallbackBehavior) {
  const size_t kBlockSize = 16;
  const size_t kTotalBlocks = 2;

  PooledAllocator allocator("TestPool", kBlockSize, kTotalBlocks);
  ASSERT_EQ(allocator.pools_for_testing().size(), 1U);
  const FixedBlockPool& pool = *allocator.pools_for_testing()[0];

  // 1. Should allocate from pool.
  void* pool_ptr1 = allocator.Allocate(kBlockSize);
  ASSERT_NE(pool_ptr1, nullptr);
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 1U);

  void* pool_ptr2 = allocator.Allocate(kBlockSize);
  ASSERT_NE(pool_ptr2, nullptr);
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 0U);

  // 2. Pool is full. Should fallback to heap.
  void* fallback_ptr = allocator.Allocate(kBlockSize);
  ASSERT_NE(fallback_ptr, nullptr);
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 0U);

  // 3. Oversized allocation. Should fallback to heap.
  void* oversized_ptr = allocator.Allocate(kBlockSize * 2);
  ASSERT_NE(oversized_ptr, nullptr);

  // 4. Free everything.
  allocator.Free(fallback_ptr);  // heap
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 0U);

  allocator.Free(oversized_ptr);  // heap

  allocator.Free(pool_ptr1);  // pool
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 1U);

  allocator.Free(pool_ptr2);  // pool
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 2U);
}

TEST(PooledAllocatorTest, N_TieredRoutingAndNoFallback) {
  const size_t kSmallBlockSize = 16;
  const size_t kSmallTotalBlocks = 2;
  const size_t kLargeBlockSize = 128;
  const size_t kLargeTotalBlocks = 2;

  // Configure multiple pools. They should be sorted automatically if we pass
  // them out of order, but let's pass them in order first.
  PooledAllocator allocator(std::vector<PooledAllocator::PoolConfig>{
      {"Pool_Large", kLargeBlockSize, kLargeTotalBlocks},
      {"Pool_Small", kSmallBlockSize, kSmallTotalBlocks}});

  ASSERT_EQ(allocator.pools_for_testing().size(), 2U);
  // Verification that they were sorted ascending by block size:
  const FixedBlockPool& small_pool = *allocator.pools_for_testing()[0];
  const FixedBlockPool& large_pool = *allocator.pools_for_testing()[1];
  EXPECT_EQ(small_pool.block_size(), kSmallBlockSize);
  EXPECT_EQ(large_pool.block_size(), kLargeBlockSize);

  // 1. Allocate small block (should go to small pool).
  void* small_ptr1 = allocator.Allocate(10);
  ASSERT_NE(small_ptr1, nullptr);
  EXPECT_EQ(small_pool.GetInfoForTesting().free_blocks, 1U);
  EXPECT_EQ(large_pool.GetInfoForTesting().free_blocks, 2U);

  // 2. Allocate another small block (fills small pool).
  void* small_ptr2 = allocator.Allocate(16);
  ASSERT_NE(small_ptr2, nullptr);
  EXPECT_EQ(small_pool.GetInfoForTesting().free_blocks, 0U);

  // 3. Allocate third small block. Small pool is exhausted, and since we use
  // strict routing, it should NOT overflow into the large pool. It must
  // fallback to the heap directly.
  void* small_fallback_ptr = allocator.Allocate(8);
  ASSERT_NE(small_fallback_ptr, nullptr);
  EXPECT_EQ(small_pool.GetInfoForTesting().free_blocks, 0U);
  EXPECT_EQ(large_pool.GetInfoForTesting().free_blocks,
            2U);  // Large pool remains untouched!

  // 4. Allocate large block (should go to large pool).
  void* large_ptr1 = allocator.Allocate(100);
  ASSERT_NE(large_ptr1, nullptr);
  EXPECT_EQ(large_pool.GetInfoForTesting().free_blocks, 1U);

  // Free everything.
  allocator.Free(small_ptr1);
  EXPECT_EQ(small_pool.GetInfoForTesting().free_blocks, 1U);

  allocator.Free(small_ptr2);
  EXPECT_EQ(small_pool.GetInfoForTesting().free_blocks, 2U);

  allocator.Free(small_fallback_ptr);  // heap
  EXPECT_EQ(small_pool.GetInfoForTesting().free_blocks, 2U);

  allocator.Free(large_ptr1);
  EXPECT_EQ(large_pool.GetInfoForTesting().free_blocks, 2U);
}

}  // namespace
}  // namespace starboard

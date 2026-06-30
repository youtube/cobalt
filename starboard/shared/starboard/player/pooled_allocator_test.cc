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
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

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
    void* ptr = pool.Allocate(kBlockSize);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(pool.IsFromPool(ptr));
    allocated_blocks.push_back(ptr);
  }
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 0);

  // 2. Try to allocate one more. Should return nullptr (no fallback).
  void* extra_ptr = pool.Allocate(kBlockSize);
  EXPECT_EQ(extra_ptr, nullptr);
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 0);

  // 3. Try to allocate with size exceeding block size. Should return nullptr.
  void* large_ptr = pool.Allocate(kBlockSize * 2);
  EXPECT_EQ(large_ptr, nullptr);

  // 4. Freeing nullptr should return false.
  EXPECT_FALSE(pool.Free(extra_ptr));
  EXPECT_FALSE(pool.Free(large_ptr));

  // 6. Free all pool blocks.
  for (void* ptr : allocated_blocks) {
    pool.Free(ptr);
  }
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, kTotalBlocks);

  // 7. Allocate again. Should recycle.
  void* ptr = pool.Allocate(kBlockSize);
  EXPECT_NE(ptr, nullptr);
  EXPECT_TRUE(pool.IsFromPool(ptr));
  pool.Free(ptr);
}

TEST(FixedBlockPoolTest, IsFromPool) {
  const size_t kBlockSize = 16;
  const size_t kTotalBlocks = 2;
  FixedBlockPool pool("TestPool", kBlockSize, kTotalBlocks);

  void* ptr1 = pool.Allocate(kBlockSize);
  void* ptr2 = pool.Allocate(kBlockSize);

  ASSERT_NE(ptr1, nullptr);
  ASSERT_NE(ptr2, nullptr);

  EXPECT_TRUE(pool.IsFromPool(ptr1));
  EXPECT_TRUE(pool.IsFromPool(ptr2));

  // Null pointer.
  EXPECT_FALSE(pool.IsFromPool(nullptr));

  // Const pointer.
  const void* const_ptr = ptr1;
  EXPECT_TRUE(pool.IsFromPool(const_ptr));

  // Heap pointer.
  int* heap_ptr = new int;
  EXPECT_FALSE(pool.IsFromPool(heap_ptr));
  delete heap_ptr;

  // Misaligned pointer (inside the pool but not at block start).
  uint8_t* misaligned_ptr = static_cast<uint8_t*>(ptr1) + 1;
  EXPECT_FALSE(pool.IsFromPool(misaligned_ptr));

  // Pointer past the end of the pool.
  uint8_t* past_end_ptr =
      static_cast<uint8_t*>(ptr1) + kBlockSize * kTotalBlocks;
  EXPECT_FALSE(pool.IsFromPool(past_end_ptr));

  pool.Free(ptr1);
  pool.Free(ptr2);
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
          void* ptr = pool.Allocate(kBlockSize);
          ASSERT_NE(ptr, nullptr);
          ASSERT_TRUE(pool.IsFromPool(ptr));
          allocated.push_back(ptr);
        }
        std::this_thread::yield();
        for (void* ptr : allocated) {
          pool.Free(ptr);
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
  void* ptr = pool.Allocate(16);
  void* misaligned = static_cast<uint8_t*>(ptr) + 1;
  EXPECT_DEATH_IF_SUPPORTED(pool.Free(misaligned), "");
  pool.Free(ptr);  // Cleanup
}

// ============================================================================
// DualBlockPool Tests
// ============================================================================

TEST(DualBlockPoolTest, BasicRoutingAndNoFallback) {
  const size_t kSmallBlockSize = 16;
  const size_t kSmallTotalBlocks = 2;
  const size_t kLargeBlockSize = 128;
  const size_t kLargeTotalBlocks = 2;

  // Constructor takes small and large configs explicitly.
  DualBlockPool pool("TestPool", {kSmallBlockSize, kSmallTotalBlocks},
                     {kLargeBlockSize, kLargeTotalBlocks});

  auto states = pool.GetStatesForTesting();
  ASSERT_EQ(states.size(), 2);
  EXPECT_EQ(states[0].block_size, kSmallBlockSize);
  EXPECT_EQ(states[1].block_size, kLargeBlockSize);

  // 1. Allocate small block (should go to small pool).
  void* small_ptr1 = pool.Allocate(10);
  ASSERT_NE(small_ptr1, nullptr);
  states = pool.GetStatesForTesting();
  EXPECT_EQ(states[0].free_blocks, 1);
  EXPECT_EQ(states[1].free_blocks, 2);

  // 2. Allocate another small block (fills small pool).
  void* small_ptr2 = pool.Allocate(16);
  ASSERT_NE(small_ptr2, nullptr);
  states = pool.GetStatesForTesting();
  EXPECT_EQ(states[0].free_blocks, 0);

  // 3. Allocate third small block. Small pool is exhausted, should return
  // nullptr (no heap fallback in pure pool).
  void* small_ptr3 = pool.Allocate(8);
  EXPECT_EQ(small_ptr3, nullptr);
  states = pool.GetStatesForTesting();
  EXPECT_EQ(states[0].free_blocks, 0);
  EXPECT_EQ(states[1].free_blocks, 2);  // Large pool remains untouched.

  // 4. Allocate large block (should go to large pool).
  void* large_ptr1 = pool.Allocate(100);
  ASSERT_NE(large_ptr1, nullptr);
  states = pool.GetStatesForTesting();
  EXPECT_EQ(states[1].free_blocks, 1);

  // 5. Allocate oversized block (exceeds large pool block size). Should return
  // nullptr.
  void* oversized_ptr = pool.Allocate(200);
  EXPECT_EQ(oversized_ptr, nullptr);
  states = pool.GetStatesForTesting();
  EXPECT_EQ(states[1].free_blocks, 1);  // Large pool free blocks unchanged.

  // Free everything.
  EXPECT_TRUE(pool.Free(small_ptr1));
  states = pool.GetStatesForTesting();
  EXPECT_EQ(states[0].free_blocks, 1);

  EXPECT_TRUE(pool.Free(small_ptr2));
  states = pool.GetStatesForTesting();
  EXPECT_EQ(states[0].free_blocks, 2);

  // Freeing a pointer not from pool should return false.
  int x;
  EXPECT_FALSE(pool.Free(&x));

  EXPECT_TRUE(pool.Free(large_ptr1));
  states = pool.GetStatesForTesting();
  EXPECT_EQ(states[1].free_blocks, 2);
}

TEST(DualBlockPoolTest, ThreadSafety) {
  const size_t kSmallBlockSize = 32;
  const size_t kSmallTotalBlocks = 10;
  const size_t kLargeBlockSize = 256;
  const size_t kLargeTotalBlocks = 5;

  DualBlockPool pool("TestPool", {kSmallBlockSize, kSmallTotalBlocks},
                     {kLargeBlockSize, kLargeTotalBlocks});

  const int kNumThreads = 4;
  const int kIterations = 100;

  std::vector<std::thread> threads;
  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&pool]() {
      for (int i = 0; i < kIterations; ++i) {
        // Allocate 1 small and 1 large.
        void* s_ptr = pool.Allocate(kSmallBlockSize);
        void* l_ptr = pool.Allocate(kLargeBlockSize);

        ASSERT_NE(s_ptr, nullptr);
        ASSERT_NE(l_ptr, nullptr);

        std::this_thread::yield();

        EXPECT_TRUE(pool.Free(s_ptr));
        EXPECT_TRUE(pool.Free(l_ptr));
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  auto states = pool.GetStatesForTesting();
  EXPECT_EQ(states[0].free_blocks, kSmallTotalBlocks);
  EXPECT_EQ(states[1].free_blocks, kLargeTotalBlocks);
}

// ============================================================================
// PooledAllocator Tests
// ============================================================================

TEST(PooledAllocatorTest, FallbackBehavior) {
  const size_t kBlockSize = 16;
  const size_t kTotalBlocks = 2;

  PooledAllocator<FixedBlockPool> allocator("TestPool", kBlockSize,
                                            kTotalBlocks);
  const FixedBlockPool& pool = allocator.pool();

  // 1. Should allocate from pool.
  void* pool_ptr1 = allocator.Allocate(kBlockSize);
  ASSERT_NE(pool_ptr1, nullptr);
  EXPECT_TRUE(pool.IsFromPool(pool_ptr1));
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 1);

  void* pool_ptr2 = allocator.Allocate(kBlockSize);
  ASSERT_NE(pool_ptr2, nullptr);
  EXPECT_TRUE(pool.IsFromPool(pool_ptr2));
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 0);

  // 2. Pool is full. Should fallback to heap.
  void* fallback_ptr = allocator.Allocate(kBlockSize);
  ASSERT_NE(fallback_ptr, nullptr);
  EXPECT_FALSE(pool.IsFromPool(fallback_ptr));
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 0);

  // 3. Oversized allocation. Should fallback to heap.
  void* oversized_ptr = allocator.Allocate(kBlockSize * 2);
  ASSERT_NE(oversized_ptr, nullptr);
  EXPECT_FALSE(pool.IsFromPool(oversized_ptr));

  // 4. Free everything.
  allocator.Free(fallback_ptr);  // heap
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 0);

  allocator.Free(oversized_ptr);  // heap

  allocator.Free(pool_ptr1);  // pool
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 1);

  allocator.Free(pool_ptr2);  // pool
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 2);
}

}  // namespace
}  // namespace starboard

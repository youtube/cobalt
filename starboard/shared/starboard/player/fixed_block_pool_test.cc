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

#include "starboard/shared/starboard/player/fixed_block_pool.h"

#include <thread>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

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

  // 2. Try to allocate one more. Should fallback to heap (not from pool).
  void* extra_ptr = pool.Allocate(kBlockSize);
  ASSERT_NE(extra_ptr, nullptr);
  EXPECT_FALSE(pool.IsFromPool(extra_ptr));
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 0);

  // 3. Try to allocate with size exceeding block size. Should fallback to heap.
  void* large_ptr = pool.Allocate(kBlockSize * 2);
  ASSERT_NE(large_ptr, nullptr);
  EXPECT_FALSE(pool.IsFromPool(large_ptr));

  // 4. Free extra_ptr (heap). Should be deleted, not returned to pool.
  pool.Free(extra_ptr);
  EXPECT_EQ(pool.GetInfoForTesting().free_blocks, 0);

  // 5. Free large_ptr (heap). Should be deleted.
  pool.Free(large_ptr);

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

TEST_F(FixedBlockPoolDeathTest, DeathTest_FreeInvalidPointer) {
  FixedBlockPool pool("TestPool", 16, 2);
  int x;
  EXPECT_DEATH_IF_SUPPORTED(pool.Free(&x), "");
}

}  // namespace
}  // namespace starboard

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

#include "starboard/shared/starboard/player/fixed_size_memory_pool.h"

#include <thread>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(FixedSizeMemoryPoolTest, BasicAllocationAndFree) {
  const size_t kBlockSize = 32;
  const size_t kCapacity = 4;
  FixedSizeMemoryPool pool("TestPool", kBlockSize, kCapacity);

  EXPECT_EQ(pool.block_size(), kBlockSize);
  EXPECT_EQ(pool.capacity(), kCapacity);
  EXPECT_EQ(pool.free_list_size(), kCapacity);

  std::vector<void*> allocated_blocks;

  // 1. Allocate all blocks.
  for (size_t i = 0; i < kCapacity; ++i) {
    void* ptr = pool.Allocate();
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(pool.IsFromPool(ptr));
    allocated_blocks.push_back(ptr);
  }
  EXPECT_EQ(pool.free_list_size(), 0);

  // 2. Try to allocate one more. Should return nullptr (no fallback).
  void* extra_ptr = pool.Allocate();
  EXPECT_EQ(extra_ptr, nullptr);

  // 3. Free all blocks.
  for (void* ptr : allocated_blocks) {
    pool.Free(ptr);
  }
  EXPECT_EQ(pool.free_list_size(), kCapacity);

  // 4. Allocate again.
  void* ptr = pool.Allocate();
  EXPECT_NE(ptr, nullptr);
  EXPECT_TRUE(pool.IsFromPool(ptr));
  pool.Free(ptr);
}

TEST(FixedSizeMemoryPoolTest, IsFromPool) {
  const size_t kBlockSize = 16;
  const size_t kCapacity = 2;
  FixedSizeMemoryPool pool("TestPool", kBlockSize, kCapacity);

  void* ptr1 = pool.Allocate();
  void* ptr2 = pool.Allocate();

  ASSERT_NE(ptr1, nullptr);
  ASSERT_NE(ptr2, nullptr);

  EXPECT_TRUE(pool.IsFromPool(ptr1));
  EXPECT_TRUE(pool.IsFromPool(ptr2));

  // Null pointer.
  EXPECT_FALSE(pool.IsFromPool(nullptr));

  // Heap pointer.
  int* heap_ptr = new int;
  EXPECT_FALSE(pool.IsFromPool(heap_ptr));
  delete heap_ptr;

  // Misaligned pointer (inside the pool but not at block start).
  uint8_t* misaligned_ptr = static_cast<uint8_t*>(ptr1) + 1;
  EXPECT_FALSE(pool.IsFromPool(misaligned_ptr));

  // Pointer past the end of the pool.
  uint8_t* past_end_ptr = static_cast<uint8_t*>(ptr1) + kBlockSize * kCapacity;
  EXPECT_FALSE(pool.IsFromPool(past_end_ptr));

  pool.Free(ptr1);
  pool.Free(ptr2);
}

TEST(FixedSizeMemoryPoolTest, ThreadSafety) {
  const size_t kBlockSize = 64;
  const size_t kCapacity = 10;
  FixedSizeMemoryPool pool("TestPool", kBlockSize, kCapacity);

  const int kNumThreads = 4;
  const int kIterations = 100;

  std::vector<std::thread> threads;
  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&pool]() {
      for (int i = 0; i < kIterations; ++i) {
        std::vector<void*> allocated;
        // Allocate up to 2 blocks per thread.
        for (size_t j = 0; j < 2; ++j) {
          void* ptr = pool.Allocate();
          if (ptr) {
            EXPECT_TRUE(pool.IsFromPool(ptr));
            allocated.push_back(ptr);
          }
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

  EXPECT_EQ(pool.free_list_size(), kCapacity);
}

using FixedSizeMemoryPoolDeathTest = ::testing::Test;

TEST_F(FixedSizeMemoryPoolDeathTest, DeathTest_InvalidConstructorArgs) {
  EXPECT_DEATH_IF_SUPPORTED(FixedSizeMemoryPool("TestPool", 0, 10), "");
  EXPECT_DEATH_IF_SUPPORTED(FixedSizeMemoryPool("TestPool", 10, 0), "");
}

TEST_F(FixedSizeMemoryPoolDeathTest, DeathTest_FreeInvalidPointer) {
  FixedSizeMemoryPool pool("TestPool", 16, 2);
  int x;
  EXPECT_DEATH_IF_SUPPORTED(pool.Free(&x), "");
}

}  // namespace
}  // namespace starboard

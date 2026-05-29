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

#include "starboard/shared/starboard/player/buffer_pool.h"

#include <thread>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(BufferPoolTest, CustomConstructor) {
  BufferPool pool(1024, 5);
  EXPECT_EQ(pool.buffer_size(), 1024);
  EXPECT_EQ(pool.pool_size(), 5);
  EXPECT_EQ(pool.free_list_size(), 5);
}

TEST(BufferPoolTest, AllocateAndFreeWithinPool) {
  BufferPool pool(1024, 3);
  EXPECT_EQ(pool.free_list_size(), 3);

  // Allocate 1
  uint8_t* p1 = pool.Allocate(512);
  ASSERT_NE(p1, nullptr);
  EXPECT_TRUE(pool.IsFromPool(p1));
  EXPECT_EQ(pool.free_list_size(), 2);

  // Allocate 2
  uint8_t* p2 = pool.Allocate(1024);
  ASSERT_NE(p2, nullptr);
  EXPECT_TRUE(pool.IsFromPool(p2));
  EXPECT_EQ(pool.free_list_size(), 1);

  // Free 1
  pool.Free(p1, 512);
  EXPECT_EQ(pool.free_list_size(), 2);

  // Free 2
  pool.Free(p2, 1024);
  EXPECT_EQ(pool.free_list_size(), 3);
}

TEST(BufferPoolTest, PoolExhaustionReturnsNull) {
  BufferPool pool(1024, 2);

  uint8_t* p1 = pool.Allocate(512);
  uint8_t* p2 = pool.Allocate(512);
  EXPECT_EQ(pool.free_list_size(), 0);

  // Pool is full, next allocation should return nullptr
  uint8_t* p3 = pool.Allocate(512);
  EXPECT_EQ(p3, nullptr);
  EXPECT_EQ(pool.free_list_size(), 0);

  // Freeing pool allocations
  pool.Free(p1, 512);
  EXPECT_EQ(pool.free_list_size(), 1);
  pool.Free(p2, 512);
  EXPECT_EQ(pool.free_list_size(), 2);
}

TEST(BufferPoolTest, ThreadSafety) {
  const size_t kNumThreads = 8;
  const size_t kAllocationsPerThread = 100;
  BufferPool pool(1024, 20);  // Pool size is less than total allocations

  std::vector<std::thread> threads;
  for (size_t i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&pool]() {
      std::vector<uint8_t*> allocated;
      for (size_t j = 0; j < kAllocationsPerThread; ++j) {
        uint8_t* p = pool.Allocate(512);
        if (p) {
          allocated.push_back(p);
        }
      }
      // Yield to increase interleaving
      std::this_thread::yield();
      for (uint8_t* p : allocated) {
        pool.Free(p, 512);
      }
    });
  }

  for (auto& t : threads) {
    t.join();
  }

  // All buffers should be returned to the pool
  EXPECT_EQ(pool.free_list_size(), 20);
}

}  // namespace
}  // namespace starboard

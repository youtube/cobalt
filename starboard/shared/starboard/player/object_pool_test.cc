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

#include "starboard/shared/starboard/player/object_pool.h"

#include <thread>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

struct TestObject {
  int x;
  double y;
  char buffer[64];
};

TEST(ObjectPoolTest, BasicAllocationAndRecycle) {
  // Create a pool with capacity 2.
  ObjectPool pool(sizeof(TestObject), 2);
  EXPECT_EQ(pool.capacity(), 2);
  EXPECT_EQ(pool.free_list_size(), 2);

  // 1. Allocate first object. Should be from pool.
  void* ptr1 = pool.Allocate();
  ASSERT_NE(ptr1, nullptr);
  EXPECT_TRUE(pool.IsFromPreallocatedPool(ptr1));
  EXPECT_EQ(pool.free_list_size(), 1);

  // 2. Allocate second object. Should be from pool.
  void* ptr2 = pool.Allocate();
  ASSERT_NE(ptr2, nullptr);
  EXPECT_TRUE(pool.IsFromPreallocatedPool(ptr2));
  EXPECT_EQ(pool.free_list_size(), 0);

  // 3. Allocate third object. Should fallback to heap (not from pool).
  void* ptr3 = pool.Allocate();
  ASSERT_NE(ptr3, nullptr);
  EXPECT_FALSE(pool.IsFromPreallocatedPool(ptr3));
  EXPECT_EQ(pool.free_list_size(), 0);

  // 4. Free ptr1 (pool). Should return to pool.
  pool.Free(ptr1);
  EXPECT_EQ(pool.free_list_size(), 1);

  // 5. Free ptr3 (heap). Should be deleted, not returned to pool.
  pool.Free(ptr3);
  EXPECT_EQ(pool.free_list_size(), 1);

  // 6. Allocate again. Should recycle ptr1.
  void* ptr4 = pool.Allocate();
  EXPECT_EQ(ptr1, ptr4);
  EXPECT_EQ(pool.free_list_size(), 0);

  // Clean up remaining.
  pool.Free(ptr2);
  pool.Free(ptr4);
  EXPECT_EQ(pool.free_list_size(), 2);
}

TEST(ObjectPoolTest, ThreadSafety) {
  const int kCapacity = 20;
  ObjectPool pool(sizeof(TestObject), kCapacity);
  const int kNumThreads = 8;
  const int kAllocationsPerThread = 100;

  std::vector<std::thread> threads;
  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([&pool]() {
      std::vector<void*> allocated;
      for (int i = 0; i < kAllocationsPerThread; ++i) {
        void* ptr = pool.Allocate();
        ASSERT_NE(ptr, nullptr);
        TestObject* obj = new (ptr) TestObject();
        obj->x = i;
        allocated.push_back(ptr);
      }

      // Free half of them
      for (size_t i = 0; i < allocated.size() / 2; ++i) {
        TestObject* obj = static_cast<TestObject*>(allocated[i]);
        obj->~TestObject();
        pool.Free(allocated[i]);
      }

      // Allocate again
      for (size_t i = 0; i < allocated.size() / 2; ++i) {
        void* ptr = pool.Allocate();
        ASSERT_NE(ptr, nullptr);
        TestObject* obj = new (ptr) TestObject();
        obj->x = 999;
        allocated[i] = ptr;
      }

      // Clean up all
      for (void* ptr : allocated) {
        TestObject* obj = static_cast<TestObject*>(ptr);
        obj->~TestObject();
        pool.Free(ptr);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // All pool buffers should be returned to the pool.
  // Fallback allocations should have been deleted.
  EXPECT_EQ(pool.free_list_size(), kCapacity);
}

}  // namespace
}  // namespace starboard

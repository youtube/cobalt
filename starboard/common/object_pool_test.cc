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

#include "starboard/common/object_pool.h"

#include <thread>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace common {
namespace {

struct TestObject {
  int x;
  double y;
  char buffer[64];
};

TEST(ObjectPoolTest, BasicAllocationAndRecycle) {
  ObjectPool<TestObject>* pool = ObjectPool<TestObject>::GetInstance();

  // 1. Allocate first object. Should be new memory.
  void* ptr1 = pool->Allocate();
  ASSERT_NE(ptr1, nullptr);

  // Initialize the memory to make sure it's writable.
  TestObject* obj1 = new (ptr1) TestObject();
  obj1->x = 42;
  EXPECT_EQ(obj1->x, 42);

  // 2. Free obj1. It should go back to the pool.
  obj1->~TestObject();
  pool->Free(ptr1);

  // 3. Allocate second object. Should recycle ptr1.
  void* ptr2 = pool->Allocate();
  EXPECT_EQ(ptr1, ptr2);

  // 4. Allocate third object. Should be new memory since pool is now empty.
  void* ptr3 = pool->Allocate();
  EXPECT_NE(ptr2, ptr3);
  ASSERT_NE(ptr3, nullptr);

  // Clean up.
  pool->Free(ptr2);
  pool->Free(ptr3);
}

TEST(ObjectPoolTest, ThreadSafety) {
  ObjectPool<TestObject>* pool = ObjectPool<TestObject>::GetInstance();
  const int kNumThreads = 8;
  const int kAllocationsPerThread = 100;

  std::vector<std::thread> threads;
  for (int t = 0; t < kNumThreads; ++t) {
    threads.emplace_back([pool]() {
      std::vector<void*> allocated;
      for (int i = 0; i < kAllocationsPerThread; ++i) {
        void* ptr = pool->Allocate();
        ASSERT_NE(ptr, nullptr);
        // Write something to ensure it's valid memory
        TestObject* obj = new (ptr) TestObject();
        obj->x = i;
        allocated.push_back(ptr);
      }

      // Randomly free half of them
      for (size_t i = 0; i < allocated.size() / 2; ++i) {
        TestObject* obj = static_cast<TestObject*>(allocated[i]);
        obj->~TestObject();
        pool->Free(allocated[i]);
      }

      // Allocate again to check recycling
      for (size_t i = 0; i < allocated.size() / 2; ++i) {
        void* ptr = pool->Allocate();
        ASSERT_NE(ptr, nullptr);
        TestObject* obj = new (ptr) TestObject();
        obj->x = 999;
        // Store the recycled pointer to clean it up later
        allocated[i] = ptr;
      }

      // Clean up all remaining
      for (void* ptr : allocated) {
        TestObject* obj = static_cast<TestObject*>(ptr);
        obj->~TestObject();
        pool->Free(ptr);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

}  // namespace
}  // namespace common
}  // namespace starboard

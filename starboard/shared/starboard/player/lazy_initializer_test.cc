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

#include "starboard/shared/starboard/player/lazy_initializer.h"

#include <atomic>
#include <thread>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

// Helper class to track constructor calls.
class TestObject {
 public:
  static std::atomic<int> constructor_calls;

  TestObject() { constructor_calls.fetch_add(1); }

  explicit TestObject(int value) : value_(value) {
    constructor_calls.fetch_add(1);
  }

  int value() const { return value_; }

 private:
  int value_ = 0;
};

std::atomic<int> TestObject::constructor_calls{0};

class LazyInitializerTest : public ::testing::Test {
 protected:
  void SetUp() override { TestObject::constructor_calls.store(0); }
};

TEST_F(LazyInitializerTest, BasicInitialization) {
  LazyInitializer<TestObject> initializer;

  EXPECT_EQ(initializer.GetIfInitialized(), nullptr);
  EXPECT_EQ(TestObject::constructor_calls.load(), 0);

  TestObject* obj1 = initializer.Get();
  ASSERT_NE(obj1, nullptr);
  EXPECT_EQ(TestObject::constructor_calls.load(), 1);
  EXPECT_EQ(initializer.GetIfInitialized(), obj1);

  TestObject* obj2 = initializer.Get();
  EXPECT_EQ(obj2, obj1);
  EXPECT_EQ(TestObject::constructor_calls.load(), 1);
}

TEST_F(LazyInitializerTest, ArgumentsPassing) {
  LazyInitializer<TestObject> initializer;
  const int kExpectedValue = 42;

  TestObject* obj = initializer.Get(kExpectedValue);
  ASSERT_NE(obj, nullptr);
  EXPECT_EQ(obj->value(), kExpectedValue);
  EXPECT_EQ(TestObject::constructor_calls.load(), 1);
}

TEST_F(LazyInitializerTest, ConcurrentInitialization) {
  LazyInitializer<TestObject> initializer;
  const int kNumThreads = 10;
  std::vector<std::thread> threads;
  std::vector<TestObject*> results(kNumThreads, nullptr);
  std::atomic<bool> start_signal{false};

  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&initializer, &results, &start_signal, i]() {
      while (!start_signal.load()) {
        std::this_thread::yield();
      }
      results[i] = initializer.Get();
    });
  }

  // Release threads to call Get() simultaneously.
  start_signal.store(true);

  for (auto& t : threads) {
    t.join();
  }

  // Verify that only one object was created.
  EXPECT_EQ(TestObject::constructor_calls.load(), 1);

  // Verify that all threads got the same pointer.
  TestObject* first_result = results[0];
  ASSERT_NE(first_result, nullptr);
  for (int i = 1; i < kNumThreads; ++i) {
    EXPECT_EQ(results[i], first_result);
  }
}

}  // namespace
}  // namespace starboard

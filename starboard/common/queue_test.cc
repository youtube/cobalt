// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/queue.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "starboard/common/thread.h"

namespace starboard {
namespace {

constexpr int kMillisInUs = 1'000;
constexpr int kSecInUs = 1'000 * kMillisInUs;

class TestObject {
 public:
  explicit TestObject(const int id) : id_(id) {}
  ~TestObject() = default;

  int id() const { return id_; }

 private:
  const int id_;
};

using UniqueTestObject = std::unique_ptr<TestObject>;

template <typename T>
class QueueTest : public ::testing::Test {
 protected:
  QueueTest() = default;
  ~QueueTest() override = default;

  Queue<T> queue_;
};

using QueueIntTest = QueueTest<int>;
using QueuePointerTest = QueueTest<TestObject*>;
using QueueUniquePointerTest = QueueTest<UniqueTestObject>;

class PutterThread : public Thread {
 public:
  PutterThread(Queue<int>* queue, int value, int64_t delay_microseconds)
      : Thread("PutterThread"),
        queue_(queue),
        value_(value),
        delay_microseconds_(delay_microseconds) {}

  void Run() override {
    Thread::Sleep(delay_microseconds_);  // Thread::Sleep is static
    queue_->Put(value_);
  }

 private:
  Queue<int>* const queue_;
  const int value_;
  const int64_t delay_microseconds_;
};

class WakerThread : public Thread {
 public:
  WakerThread(Queue<int>* queue, int64_t delay_microseconds)
      : Thread("WakerThread"),
        queue_(queue),
        delay_microseconds_(delay_microseconds) {}

  void Run() override {
    Thread::Sleep(delay_microseconds_);  // Thread::Sleep is static
    queue_->Wake();
  }

 private:
  Queue<int>* const queue_;
  const int64_t delay_microseconds_;
};

class ConsumerThread : public Thread {
 public:
  ConsumerThread(Queue<int>* queue,
                 std::atomic<int>* consumed_count,
                 int total_items,
                 std::atomic<bool>* producers_finished)
      : Thread("ConsumerThread"),
        queue_(queue),
        consumed_count_(consumed_count),
        total_items_(total_items),
        producers_finished_(producers_finished) {}

  void Run() override {
    while (consumed_count_->load() < total_items_) {
      int item = queue_->GetTimed(kMillisInUs * 10);  // Use a timeout
      if (item != 0) {
        consumed_count_->fetch_add(1);
      }
      // If producers are done and we timed out, break the loop.
      if (producers_finished_->load() && item == 0) {
        break;
      }
    }
  }

 private:
  Queue<int>* const queue_;
  std::atomic<int>* const consumed_count_;
  const int total_items_;
  std::atomic<bool>* const producers_finished_;
};

class ProducerConcurrentThread : public Thread {
 public:
  ProducerConcurrentThread(const std::string& name,
                           Queue<int>* queue,
                           std::atomic<int>* produced_count,
                           const int items_per_producer)
      : Thread(name),
        queue_(queue),
        produced_count_(produced_count),
        items_per_producer_(items_per_producer) {}

  void Run() override {
    for (int j = 0; j < items_per_producer_; ++j) {
      // Put non-zero items so the consumer can distinguish a real item from a
      // wake-up, as Get() returns 0 in that case.
      queue_->Put(j + 1);
      produced_count_->fetch_add(1);
    }
  }

 private:
  Queue<int>* const queue_;
  std::atomic<int>* const produced_count_;
  const int items_per_producer_;
};

TEST_F(QueueIntTest, PollEmptyQueueReturnsDefault) {
  EXPECT_EQ(0, queue_.Poll());  // Default for int is 0
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueuePointerTest, PollEmptyQueueReturnsNullptr) {
  EXPECT_EQ(nullptr, queue_.Poll());  // Default for pointer is nullptr
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueueIntTest, PutAndPoll) {
  queue_.Put(10);
  EXPECT_EQ(1, queue_.Size());
  EXPECT_EQ(10, queue_.Poll());
  EXPECT_EQ(0, queue_.Size());
  EXPECT_EQ(0, queue_.Poll());  // Should be empty now
}

TEST_F(QueuePointerTest, PutAndPollPointers) {
  auto obj1_owner = std::make_unique<TestObject>(1);
  TestObject* obj1 = obj1_owner.get();
  auto obj2_owner = std::make_unique<TestObject>(2);
  TestObject* obj2 = obj2_owner.get();

  queue_.Put(obj1);
  queue_.Put(obj2);

  EXPECT_EQ(2, queue_.Size());
  EXPECT_EQ(obj1, queue_.Poll());
  EXPECT_EQ(1, queue_.Size());
  EXPECT_EQ(obj2, queue_.Poll());
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueueUniquePointerTest, PutAndPollUniquePointers) {
  auto obj1 = std::make_unique<TestObject>(1);
  auto obj2 = std::make_unique<TestObject>(2);

  TestObject* raw_obj1 = obj1.get();
  TestObject* raw_obj2 = obj2.get();

  queue_.Put(std::move(obj1));
  queue_.Put(std::move(obj2));

  EXPECT_EQ(2, queue_.Size());
  UniqueTestObject retrieved_obj1 = queue_.Poll();
  EXPECT_EQ(raw_obj1, retrieved_obj1.get());
  EXPECT_EQ(1, queue_.Size());
  UniqueTestObject retrieved_obj2 = queue_.Poll();
  EXPECT_EQ(raw_obj2, retrieved_obj2.get());
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueueIntTest, GetBlocksUntilItemIsAvailable) {
  int received_value = 0;
  PutterThread putter_thread(&queue_, 42, kMillisInUs * 100);

  putter_thread.Start();
  received_value = queue_.Get();
  putter_thread.Join();

  EXPECT_EQ(42, received_value);
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueueIntTest, GetTimedReturnsDefaultOnTimeout) {
  int received_value = queue_.GetTimed(kMillisInUs * 50);
  EXPECT_EQ(0, received_value);
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueueIntTest, GetTimedRetrievesItemWithinTimeout) {
  int received_value = 0;
  PutterThread putter_thread(&queue_, 99, kMillisInUs * 10);

  putter_thread.Start();
  received_value = queue_.GetTimed(kMillisInUs * 500);
  putter_thread.Join();

  EXPECT_EQ(99, received_value);
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueueIntTest, WakeWakesUpGetCall) {
  int received_value = 0;
  WakerThread waker_thread(&queue_, kMillisInUs * 100);

  waker_thread.Start();
  received_value = queue_.Get();  // Should be woken up empty-handed
  waker_thread.Join();

  EXPECT_EQ(0, received_value);  // Default value for int
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueueIntTest, WakeWakesUpGetTimedCall) {
  int received_value = 0;
  WakerThread waker_thread(&queue_, kMillisInUs * 100);

  waker_thread.Start();
  received_value = queue_.GetTimed(kSecInUs);
  waker_thread.Join();

  EXPECT_EQ(0, received_value);  // Default value for int
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueueIntTest, MultiplePutsAndGets) {
  for (int i = 0; i < 100; ++i) {
    queue_.Put(i);
  }
  EXPECT_EQ(100, queue_.Size());

  for (int i = 0; i < 100; ++i) {
    EXPECT_EQ(i, queue_.Get());
  }
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueueIntTest, ClearEmptiesQueue) {
  queue_.Put(1);
  queue_.Put(2);
  queue_.Put(3);
  EXPECT_EQ(3, queue_.Size());

  queue_.Clear();
  EXPECT_EQ(0, queue_.Size());
  EXPECT_EQ(0, queue_.Poll());
}

TEST_F(QueuePointerTest, RemoveSpecificItem) {
  auto obj1_owner = std::make_unique<TestObject>(1);
  TestObject* obj1 = obj1_owner.get();
  auto obj2_owner = std::make_unique<TestObject>(2);
  TestObject* obj2 = obj2_owner.get();
  auto obj3_owner = std::make_unique<TestObject>(3);
  TestObject* obj3 = obj3_owner.get();
  auto obj4_owner =
      std::make_unique<TestObject>(2);  // Duplicate id for testing
  TestObject* obj4 = obj4_owner.get();

  queue_.Put(obj1);
  queue_.Put(obj2);
  queue_.Put(obj3);
  queue_.Put(obj4);

  EXPECT_EQ(4, queue_.Size());
  queue_.Remove(obj2);  // Should remove the first instance of obj2

  EXPECT_EQ(3, queue_.Size());
  EXPECT_EQ(obj1, queue_.Poll());
  EXPECT_EQ(obj3, queue_.Poll());
  EXPECT_EQ(obj4, queue_.Poll());
  EXPECT_EQ(nullptr, queue_.Poll());
}

TEST_F(QueuePointerTest, RemoveNonExistentItem) {
  auto obj1_owner = std::make_unique<TestObject>(1);
  TestObject* obj1 = obj1_owner.get();
  auto obj2_owner = std::make_unique<TestObject>(2);
  TestObject* obj2 = obj2_owner.get();

  queue_.Put(obj1);
  queue_.Put(obj2);

  EXPECT_EQ(2, queue_.Size());
  auto non_existent_obj_owner = std::make_unique<TestObject>(99);
  queue_.Remove(non_existent_obj_owner.get());  // Should do nothing

  EXPECT_EQ(2, queue_.Size());
  EXPECT_EQ(obj1, queue_.Poll());
  EXPECT_EQ(obj2, queue_.Poll());
}

// Concurrency Test: Multiple threads putting and getting
TEST_F(QueueIntTest, ConcurrentPutAndGet) {
  const int kNumProducers = 5;
  const int kNumConsumers = 5;
  const int kItemsPerProducer = 100;
  const int kTotalItems = kNumProducers * kItemsPerProducer;

  std::vector<std::unique_ptr<ProducerConcurrentThread>> producer_threads;
  std::vector<std::unique_ptr<ConsumerThread>> consumer_threads;
  std::atomic<int> produced_count = 0;
  std::atomic<int> consumed_count = 0;
  std::atomic<bool> producers_finished = false;

  for (int i = 0; i < kNumProducers; ++i) {
    producer_threads.push_back(std::make_unique<ProducerConcurrentThread>(
        "ProducerThread_" + std::to_string(i), &queue_, &produced_count,
        kItemsPerProducer));
  }

  for (int i = 0; i < kNumConsumers; ++i) {
    consumer_threads.push_back(std::make_unique<ConsumerThread>(
        &queue_, &consumed_count, kTotalItems, &producers_finished));
  }

  for (auto& thread : producer_threads) {
    thread->Start();
  }
  for (auto& thread : consumer_threads) {
    thread->Start();
  }

  for (auto& thread : producer_threads) {
    thread->Join();
  }

  producers_finished = true;

  // After all producers are done, some consumers might be blocked waiting for
  // items that will never come. Wake them up so they can re-evaluate their
  // loop condition and terminate gracefully.
  for (int i = 0; i < kNumConsumers; ++i) {
    queue_.Wake();
  }

  for (auto& thread : consumer_threads) {
    thread->Join();
  }

  EXPECT_EQ(kTotalItems, produced_count.load());
  EXPECT_EQ(kTotalItems, consumed_count.load());
  EXPECT_EQ(0, queue_.Size());
}

}  // namespace
}  // namespace starboard

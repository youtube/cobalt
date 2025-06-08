#include "starboard/common/queue.h"

#include <atomic>
#include <memory>
#include <string>  // Required for std::to_string
#include <vector>
#include "gtest/gtest.h"
#include "starboard/common/thread.h"  // Use the provided Thread declaration

namespace starboard {
namespace {

constexpr int kSbTimeMillisecond = 1'000;
constexpr int kSbTimeSecond = 1'000 * kSbTimeMillisecond;

// A simple test class to use with the Queue,
// demonstrating nullable behavior and resource management.
class TestObject {
 public:
  explicit TestObject(int id) : id_(id), destroyed_(false) {}
  ~TestObject() { destroyed_ = true; }

  int id() const { return id_; }
  bool is_destroyed() const { return destroyed_; }

  // Required for std::remove in Queue::Remove
  bool operator==(const TestObject& other) const {
    return id_ == other.id_ && !destroyed_ && !other.destroyed_;
  }
  bool operator!=(const TestObject& other) const { return !(*this == other); }

 private:
  int id_;
  bool destroyed_;
};

// Custom deleter for unique_ptr to track destruction
struct TestObjectDeleter {
  void operator()(TestObject* obj) const {
    if (obj) {
      delete obj;
    }
  }
};

using UniqueTestObject = std::unique_ptr<TestObject, TestObjectDeleter>;

// Test fixture for Queue
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

// --- Helper Threads for Testing Blocking Operations ---

// Thread for putting an item into the queue
class PutterThread : public Thread {
 public:
  PutterThread(Queue<int>* queue, int value, int64_t delay_microseconds)
      : Thread("PutterThread"),
        queue_(queue),
        value_(value),
        delay_microseconds_(delay_microseconds) {}

  void Run() override {
    Thread::Sleep(delay_microseconds_);
    queue_->Put(value_);
  }

 private:
  Queue<int>* queue_;
  int value_;
  int64_t delay_microseconds_;
};

// Thread for waking up the queue
class WakerThread : public Thread {
 public:
  WakerThread(Queue<int>* queue, int64_t delay_microseconds)
      : Thread("WakerThread"),
        queue_(queue),
        delay_microseconds_(delay_microseconds) {}

  void Run() override {
    Thread::Sleep(delay_microseconds_);
    queue_->Wake();
  }

 private:
  Queue<int>* queue_;
  int64_t delay_microseconds_;
};

// Thread for consuming items from the queue in concurrent tests
class ConsumerThread : public Thread {
 public:
  ConsumerThread(Queue<int>* queue,
                 std::atomic<int>* consumed_count,
                 int total_items)
      : Thread("ConsumerThread"),
        queue_(queue),
        consumed_count_(consumed_count),
        total_items_(total_items) {}

  void Run() override {
    while (consumed_count_->load() < total_items_) {
      int item = queue_->Get();
      // Only count if it's not a default 0 returned by Wake(),
      // or if we know all items have been put. This helps avoid
      // prematurely incrementing if a consumer wakes up empty.
      if (item != 0 || consumed_count_->load() < total_items_) {
        consumed_count_->fetch_add(1);
      }
    }
  }

 private:
  Queue<int>* queue_;
  std::atomic<int>* consumed_count_;
  int total_items_;
};

// New Helper Thread for Producers in ConcurrentPutAndGet
class ProducerConcurrentThread : public Thread {
 public:
  ProducerConcurrentThread(const std::string& name,
                           Queue<int>* queue,
                           std::atomic<int>* produced_count,
                           int items_per_producer)
      : Thread(name),
        queue_(queue),
        produced_count_(produced_count),
        items_per_producer_(items_per_producer) {}

  void Run() override {
    for (int j = 0; j < items_per_producer_; ++j) {
      queue_->Put(j);
      produced_count_->fetch_add(1);
    }
  }

 private:
  Queue<int>* queue_;
  std::atomic<int>* produced_count_;
  int items_per_producer_;
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
  TestObject* obj1 = new TestObject(1);
  TestObject* obj2 = new TestObject(2);

  queue_.Put(obj1);
  queue_.Put(obj2);

  EXPECT_EQ(2, queue_.Size());
  EXPECT_EQ(obj1, queue_.Poll());
  EXPECT_EQ(1, queue_.Size());
  EXPECT_EQ(obj2, queue_.Poll());
  EXPECT_EQ(0, queue_.Size());

  delete obj1;  // Clean up allocated memory
  delete obj2;
}

TEST_F(QueueUniquePointerTest, PutAndPollUniquePointers) {
  UniqueTestObject obj1(new TestObject(1));
  UniqueTestObject obj2(new TestObject(2));

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

  EXPECT_TRUE(
      retrieved_obj1->is_destroyed());  // Should be destroyed by unique_ptr
  EXPECT_TRUE(retrieved_obj2->is_destroyed());
}

TEST_F(QueueIntTest, GetBlocksUntilItemIsAvailable) {
  int received_value = 0;
  PutterThread putter_thread(&queue_, 42, kSbTimeMillisecond * 100);

  putter_thread.Start();
  received_value = queue_.Get();
  putter_thread.Join();

  EXPECT_EQ(42, received_value);
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueueIntTest, GetTimedReturnsDefaultOnTimeout) {
  int received_value =
      queue_.GetTimed(kSbTimeMillisecond * 50);  // Short timeout
  EXPECT_EQ(0, received_value);  // Should timeout and return default
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueueIntTest, GetTimedRetrievesItemWithinTimeout) {
  int received_value = 0;
  PutterThread putter_thread(&queue_, 99,
                             kSbTimeMillisecond * 10);  // Put quickly

  putter_thread.Start();
  received_value =
      queue_.GetTimed(kSbTimeMillisecond * 500);  // Long enough timeout
  putter_thread.Join();

  EXPECT_EQ(99, received_value);
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueueIntTest, WakeWakesUpGetCall) {
  int received_value = 0;
  WakerThread waker_thread(&queue_, kSbTimeMillisecond * 100);

  waker_thread.Start();
  received_value = queue_.Get();  // Should be woken up empty-handed
  waker_thread.Join();

  EXPECT_EQ(0, received_value);  // Default value for int
  EXPECT_EQ(0, queue_.Size());
}

TEST_F(QueueIntTest, WakeWakesUpGetTimedCall) {
  int received_value = 0;
  WakerThread waker_thread(&queue_, kSbTimeMillisecond * 100);

  waker_thread.Start();
  received_value = queue_.GetTimed(kSbTimeSecond);  // Long timeout
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
  TestObject* obj1 = new TestObject(1);
  TestObject* obj2 = new TestObject(2);
  TestObject* obj3 = new TestObject(3);
  TestObject* obj4 = new TestObject(2);  // Duplicate id for testing

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

  delete obj1;
  delete obj2;  // Manually delete removed object
  delete obj3;
  delete obj4;
}

TEST_F(QueuePointerTest, RemoveNonExistentItem) {
  TestObject* obj1 = new TestObject(1);
  TestObject* obj2 = new TestObject(2);

  queue_.Put(obj1);
  queue_.Put(obj2);

  EXPECT_EQ(2, queue_.Size());
  TestObject* non_existent_obj = new TestObject(99);
  queue_.Remove(non_existent_obj);  // Should do nothing

  EXPECT_EQ(2, queue_.Size());
  EXPECT_EQ(obj1, queue_.Poll());
  EXPECT_EQ(obj2, queue_.Poll());

  delete obj1;
  delete obj2;
  delete non_existent_obj;
}

TEST_F(QueueUniquePointerTest, RemoveDoesNotDestroyUniquePointers) {
  UniqueTestObject obj1(new TestObject(1));
  UniqueTestObject obj2(new TestObject(2));
  UniqueTestObject obj3(new TestObject(3));

  TestObject* raw_obj1 = obj1.get();
  TestObject* raw_obj2 = obj2.get();
  TestObject* raw_obj3 = obj3.get();

  queue_.Put(std::move(obj1));
  queue_.Put(std::move(obj2));
  queue_.Put(std::move(obj3));

  EXPECT_EQ(3, queue_.Size());
  // In the case of unique_ptr, operator== for TestObject compares raw pointers.
  // So we need to create a temporary unique_ptr to hold the raw pointer for
  // removal. Note: This highlights the user's responsibility when using Remove
  // with non-trivial types. The removed object's ownership is effectively lost
  // by the Queue.
  UniqueTestObject temp_obj2(raw_obj2, TestObjectDeleter());
  queue_.Remove(std::move(temp_obj2));  // This will move temp_obj2 into remove,
                                        // then compare by raw pointer

  EXPECT_EQ(2, queue_.Size());
  EXPECT_EQ(raw_obj1, queue_.Poll().get());
  EXPECT_EQ(raw_obj3, queue_.Poll().get());

  // The raw_obj2 is still alive because Queue::Remove only removes it from the
  // deque, it doesn't call delete on the removed unique_ptr (since it was moved
  // out). The user of Queue has to manage the lifetime of the object
  // corresponding to the removed pointer.
  EXPECT_FALSE(raw_obj2->is_destroyed());
  delete raw_obj2;  // Manually delete the raw pointer that was 'removed' from
                    // the queue.
}

// Concurrency Test: Multiple threads putting and getting
TEST_F(QueueIntTest, ConcurrentPutAndGet) {
  const int kNumProducers = 5;
  const int kNumConsumers = 5;
  const int kItemsPerProducer = 100;
  const int kTotalItems = kNumProducers * kItemsPerProducer;

  std::vector<std::unique_ptr<ProducerConcurrentThread>> producer_threads;
  std::vector<std::unique_ptr<ConsumerThread>>
      consumer_threads;  // Using the helper ConsumerThread
  std::atomic<int> produced_count = 0;
  std::atomic<int> consumed_count = 0;

  // Producers
  for (int i = 0; i < kNumProducers; ++i) {
    producer_threads.push_back(std::make_unique<ProducerConcurrentThread>(
        "ProducerThread_" + std::to_string(i), &queue_, &produced_count,
        kItemsPerProducer));
  }

  // Consumers
  for (int i = 0; i < kNumConsumers; ++i) {
    consumer_threads.push_back(std::make_unique<ConsumerThread>(
        &queue_, &consumed_count, kTotalItems));
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
  for (auto& thread : consumer_threads) {
    thread->Join();
  }

  EXPECT_EQ(kTotalItems, produced_count.load());
  EXPECT_EQ(kTotalItems, consumed_count.load());
  EXPECT_EQ(0, queue_.Size());
}

}  // namespace
}  // namespace starboard

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

#include "starboard/common/semaphore.h"

#include <atomic>
#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "starboard/common/thread.h"
#include "starboard/common/time.h"

namespace starboard {
namespace {

constexpr int kMillisInUs = 1'000;

// Helper thread for testing blocking Take()
class TakerThread : public Thread {
 public:
  TakerThread(Semaphore* semaphore, std::atomic_bool* took_permit)
      : Thread("Taker"), semaphore_(semaphore), took_permit_(took_permit) {}

  void Run() override {
    semaphore_->Take();
    took_permit_->store(true);
  }

 private:
  Semaphore* const semaphore_;
  std::atomic_bool* const took_permit_;
};

// Helper thread for testing TakeWait() with a timeout
class TimedTakerThread : public Thread {
 public:
  TimedTakerThread(Semaphore* semaphore,
                   std::atomic_bool* const took_permit,
                   const int64_t timeout_us)
      : Thread("TimedTaker"),
        semaphore_(semaphore),
        took_permit_(took_permit),
        timeout_us_(timeout_us) {}

  void Run() override {
    bool result = semaphore_->TakeWait(timeout_us_);
    took_permit_->store(result);
  }

 private:
  Semaphore* const semaphore_;
  std::atomic_bool* const took_permit_;
  const int64_t timeout_us_;
};

// Helper thread for putting permits
class PutterThread : public Thread {
 public:
  PutterThread(Semaphore* semaphore,
               const int num_puts,
               const int64_t delay_us = 0)
      : Thread("PutterThread"),
        semaphore_(semaphore),
        num_puts_(num_puts),
        delay_us_(delay_us) {}

  void Run() override {
    if (delay_us_ > 0) {
      Thread::Sleep(delay_us_);
    }
    for (int i = 0; i < num_puts_; ++i) {
      semaphore_->Put();
    }
  }

 private:
  Semaphore* const semaphore_;
  const int num_puts_;
  const int64_t delay_us_;
};

TEST(SemaphoreTest, InitialPermitsZero) {
  Semaphore sem;
  EXPECT_FALSE(sem.TakeTry());  // Should not have any permits initially
}

TEST(SemaphoreTest, InitialPermitsNonZero) {
  Semaphore sem(5);
  EXPECT_TRUE(sem.TakeTry());  // Should have permits
  EXPECT_TRUE(sem.TakeTry());
  EXPECT_TRUE(sem.TakeTry());
  EXPECT_TRUE(sem.TakeTry());
  EXPECT_TRUE(sem.TakeTry());
  EXPECT_FALSE(sem.TakeTry());  // All permits taken
}

TEST(SemaphoreTest, PutIncreasesPermits) {
  Semaphore sem;
  EXPECT_FALSE(sem.TakeTry());
  sem.Put();
  EXPECT_TRUE(sem.TakeTry());
  EXPECT_FALSE(sem.TakeTry());  // Permit consumed
}

TEST(SemaphoreTest, TakeBlocksAndUnblocks) {
  Semaphore sem;
  std::atomic_bool took_permit(false);
  TakerThread taker_thread(&sem, &took_permit);

  taker_thread.Start();
  usleep(kMillisInUs * 50);          // Give taker thread time to block
  EXPECT_FALSE(took_permit.load());  // Should still be blocked

  sem.Put();  // Unblock the taker thread
  taker_thread.Join();

  EXPECT_TRUE(took_permit.load());
  EXPECT_FALSE(sem.TakeTry());  // Permit should be consumed
}

TEST(SemaphoreTest, TakeTryNonBlocking) {
  Semaphore sem;
  EXPECT_FALSE(sem.TakeTry());
  sem.Put();
  EXPECT_TRUE(sem.TakeTry());
  EXPECT_FALSE(sem.TakeTry());  // Permit consumed
}

TEST(SemaphoreTest, TakeWaitTimeout) {
  Semaphore sem;
  std::atomic_bool took_permit(false);
  TimedTakerThread taker_thread(&sem, &took_permit, kMillisInUs * 100);

  taker_thread.Start();
  taker_thread.Join();  // Should timeout

  EXPECT_FALSE(took_permit.load());  // Should not have taken a permit
  EXPECT_FALSE(sem.TakeTry());       // No permits added
}

TEST(SemaphoreTest, TakeWaitSuccessWithinTimeout) {
  Semaphore sem;
  std::atomic_bool took_permit(false);
  TimedTakerThread taker_thread(&sem, &took_permit, kMillisInUs * 500);
  PutterThread putter_thread(&sem, 1, kMillisInUs * 100);

  taker_thread.Start();
  putter_thread.Start();

  taker_thread.Join();
  putter_thread.Join();

  EXPECT_TRUE(took_permit.load());  // Should have taken a permit
  EXPECT_FALSE(sem.TakeTry());      // Permit consumed
}

TEST(SemaphoreTest, TakeWaitZeroTimeoutBehavesLikeTakeTry) {
  Semaphore sem;
  EXPECT_FALSE(sem.TakeWait(0));  // No permits
  sem.Put();
  EXPECT_TRUE(sem.TakeWait(0));   // Has permit
  EXPECT_FALSE(sem.TakeWait(0));  // Permit consumed
}

TEST(SemaphoreTest, TakeWaitNegativeTimeoutBehavesLikeTakeTry) {
  Semaphore sem;
  EXPECT_FALSE(sem.TakeWait(-1));  // No permits
  sem.Put();
  EXPECT_TRUE(sem.TakeWait(-1));   // Has permit
  EXPECT_FALSE(sem.TakeWait(-1));  // Permit consumed
}

TEST(SemaphoreTest, ConcurrentPutAndTake) {
  Semaphore sem;
  const int kNumPuts = 100;
  const int kNumTakes = 100;

  std::vector<std::unique_ptr<PutterThread>> putter_threads;
  std::vector<std::unique_ptr<TakerThread>> taker_threads;
  std::vector<std::atomic_bool> took_flags(kNumTakes);

  // Create putter threads
  putter_threads.push_back(std::make_unique<PutterThread>(&sem, kNumPuts));

  // Create taker threads
  for (int i = 0; i < kNumTakes; ++i) {
    took_flags[i].store(false);
    taker_threads.push_back(
        std::make_unique<TakerThread>(&sem, &took_flags[i]));
  }

  // Start all threads
  for (auto& thread : putter_threads) {
    thread->Start();
  }
  for (auto& thread : taker_threads) {
    thread->Start();
  }

  // Join all threads
  for (auto& thread : putter_threads) {
    thread->Join();
  }
  for (auto& thread : taker_threads) {
    thread->Join();
  }

  // Verify all takes were successful
  for (int i = 0; i < kNumTakes; ++i) {
    EXPECT_TRUE(took_flags[i].load())
        << "Taker thread " << i << " failed to take a permit.";
  }

  // Verify queue is empty (all permits consumed)
  EXPECT_FALSE(sem.TakeTry());
}

TEST(SemaphoreTest, MultipleTakersSinglePutter) {
  Semaphore sem;
  const int kNumTakers = 5;
  const int kNumPuts = 5;

  std::vector<std::unique_ptr<TakerThread>> taker_threads;
  std::vector<std::atomic_bool> took_flags(kNumTakers);

  for (int i = 0; i < kNumTakers; ++i) {
    took_flags[i].store(false);
    taker_threads.push_back(
        std::make_unique<TakerThread>(&sem, &took_flags[i]));
    taker_threads.back()->Start();  // Start takers, they will block
  }

  usleep(kMillisInUs * 50);  // Give takers time to block

  PutterThread putter_thread(&sem, kNumPuts);
  putter_thread.Start();
  putter_thread.Join();

  for (auto& thread : taker_threads) {
    thread->Join();
  }

  for (int i = 0; i < kNumTakers; ++i) {
    EXPECT_TRUE(took_flags[i].load())
        << "Taker thread " << i << " failed to take a permit.";
  }
  EXPECT_FALSE(sem.TakeTry());  // All permits should be consumed
}

}  // namespace
}  // namespace starboard

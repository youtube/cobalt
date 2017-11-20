// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "starboard/common/rwlock.h"
#include "starboard/configuration.h"
#include "starboard/nplb/thread_helpers.h"
#include "starboard/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

// Increasing threads by 2x increases testing time by 3x, due to write
// thread starvation. The test that relies on this number already runs
// at about ~300ms.
#define NUM_STRESS_THREADS 4

namespace starboard {
namespace nplb {

// Basic usage of RWLock.
TEST(RWLock, Use) {
  RWLock rw_lock;
  // Read lock means we can acquire it multiple times.
  rw_lock.AcquireReadLock();
  rw_lock.AcquireReadLock();
  rw_lock.ReleaseReadLock();
  rw_lock.ReleaseReadLock();

  rw_lock.AcquireWriteLock();
  rw_lock.ReleaseWriteLock();
}

// Enters a RWLock as a reader and then increments a counter indicating that it
class ReadAndSignalTestThread : public AbstractTestThread {
 public:
  struct SharedData {
    SharedData()
        : read_count(0), sema_read_acquisition(0), sema_finish_run(0) {}

    int read_count;
    RWLock rw_mutex;
    Semaphore sema_read_acquisition;
    Semaphore sema_finish_run;
  };

  explicit ReadAndSignalTestThread(SharedData* shared_data)
      : shared_data_(shared_data) {}

  void Run() override {
    shared_data_->rw_mutex.AcquireReadLock();
    // Indicate that this thread has entered the read state.
    shared_data_->read_count++;

    // Signals the this thread has finished entering the read state.
    shared_data_->sema_read_acquisition.Put();
    // Now waiting for the thread to allow us to finish.
    shared_data_->sema_finish_run.Take();
    shared_data_->read_count--;
    shared_data_->rw_mutex.ReleaseReadLock();
  }

 private:
  SharedData* shared_data_;
};

// Tests the expectation that two reader threads can enter a rw_lock.
TEST(RWLock, ReadAcquisitionTwoThreads) {
  ReadAndSignalTestThread::SharedData data;

  ReadAndSignalTestThread read_thread(&data);
  read_thread.Start();

  // Wait until the reader thread has finished entering into the read state.
  data.sema_read_acquisition.Take();
  EXPECT_EQ(1, data.read_count);

  // The rw_lock is still being held now, therefore we test that we can
  // acquire another access to the read.
  data.rw_mutex.AcquireReadLock();
  data.rw_mutex.ReleaseReadLock();

  // Release the other thread so that it can exit its run function.
  data.sema_finish_run.Put();
  read_thread.Join();

  EXPECT_EQ(0, data.read_count);
}

// Tests the expectation that a read lock will be blocked for X milliseconds
// while the thread is holding the write lock.
class ThreadHoldsWriteLockForTime : public AbstractTestThread {
 public:
  struct SharedData {
    explicit SharedData(SbTime time_hold) : time_to_hold(time_hold) {}
    SbTime time_to_hold;
    Semaphore signal_write_lock;
    RWLock rw_lock;
  };
  explicit ThreadHoldsWriteLockForTime(SharedData* shared_data)
      : shared_data_(shared_data) {}

  void Run() override {
    ScopedWriteLock write_lock(&shared_data_->rw_lock);
    shared_data_->signal_write_lock.Put();
    SbThreadSleep(shared_data_->time_to_hold);
  }

 private:
  SharedData* shared_data_;
};
TEST(RWLock, HoldsLockForTime) {
  const SbTime kTimeToHold = kSbTimeMillisecond * 5;
  const SbTime kAllowedError = kSbTimeMillisecond * 10;

  ThreadHoldsWriteLockForTime::SharedData shared_data(kTimeToHold);
  ThreadHoldsWriteLockForTime thread(&shared_data);

  thread.Start();
  shared_data.signal_write_lock.Take();  // write lock was taken, start timer.
  const SbTime start_time = SbTimeGetMonotonicNow();
  shared_data.rw_lock.AcquireReadLock();  // Blocked by thread for kTimeToHold.
  shared_data.rw_lock.ReleaseReadLock();
  const SbTime delta_time = SbTimeGetMonotonicNow() - start_time;
  thread.Join();

  SbTime time_diff = delta_time - kTimeToHold;
  if (time_diff < 0) {
    time_diff = -time_diff;
  }
  EXPECT_LT(time_diff, kAllowedError);
}

// This thread tests RWLock by generating numbers and writing to a
// shared set<int32_t>. Additionally readbacks are interleaved in writes.
class ThreadRWLockStressTest : public AbstractTestThread {
 public:
  struct SharedData {
    RWLock rw_lock;
    std::set<int32_t> data;
  };

  ThreadRWLockStressTest(int32_t begin_value,
                         int32_t end_value,
                         SharedData* shared_data)
      : begin_value_(begin_value),
        end_value_(end_value),
        shared_data_(shared_data) {}

  void Run() override {
    SbThread current_thread = SbThreadGetCurrent();
    SB_UNREFERENCED_PARAMETER(current_thread);

    for (int32_t i = begin_value_; i < end_value_; ++i) {
      DoReadAll();
      DoWrite(i);
    }
  }

 private:
  void DoReadAll() {
    typedef std::set<int32_t>::const_iterator ConstIterator;
    volatile int32_t dummy = 0;

    shared_data_->rw_lock.AcquireReadLock();
    for (ConstIterator it = shared_data_->data.begin();
         it != shared_data_->data.end(); ++it) {
      dummy = *it;
    }
    shared_data_->rw_lock.ReleaseReadLock();
  }

  void DoWrite(int32_t value) {
    shared_data_->rw_lock.AcquireWriteLock();
    const bool added = shared_data_->data.insert(value).second;
    shared_data_->rw_lock.ReleaseWriteLock();
    ASSERT_TRUE(added);
  }

  int32_t begin_value_;
  int32_t end_value_;
  SharedData* shared_data_;
};

TEST(RWLock, RWLockStressTest) {
  const int32_t kNumValuesEachThread = 500;
  // Expected number of values that will be generated by all threads.
  const size_t kTotalValuesGenerated =
      kNumValuesEachThread * NUM_STRESS_THREADS;

  ThreadRWLockStressTest::SharedData shared_data;
  std::vector<AbstractTestThread*> threads;

  for (int i = 0; i < NUM_STRESS_THREADS; ++i) {
    int32_t start_value = i * kNumValuesEachThread;
    int32_t end_value = (i + 1) * kNumValuesEachThread;
    ThreadRWLockStressTest* thread =
        new ThreadRWLockStressTest(start_value, end_value, &shared_data);
    threads.push_back(thread);
  }

  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->Start();
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->Join();
    delete threads[i];
  }

  std::vector<int32_t> values(shared_data.data.begin(), shared_data.data.end());

  // We expect that the generators will generate [0...kTotalValuesGenerated).
  ASSERT_EQ(kTotalValuesGenerated, values.size());
  for (size_t i = 0; i < values.size(); ++i) {
    int32_t value = values[i];
    ASSERT_EQ(value, i);
  }
}

}  // namespace nplb
}  // namespace starboard

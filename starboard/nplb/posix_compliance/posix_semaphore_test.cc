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

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <time.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Tests the basic initialization and destruction of a semaphore.
TEST(PosixSemaphoreTest, InitDestroy) {
  sem_t sem;
  // Initialize a semaphore that is not shared between processes (pshared=0)
  // with an initial value of 1.
  int ret = sem_init(&sem, 0, 1);
  EXPECT_EQ(ret, 0) << "sem_init failed: " << strerror(errno);

  if (ret == 0) {
    ret = sem_destroy(&sem);
    EXPECT_EQ(ret, 0) << "sem_destroy failed: " << strerror(errno);
  }
}

// Tests a simple wait-and-post sequence.
TEST(PosixSemaphoreTest, WaitAndPost) {
  sem_t sem;
  // Initialize with a value of 1, so the first wait should succeed.
  int ret = sem_init(&sem, 0, 1);
  ASSERT_EQ(ret, 0) << "sem_init failed: " << strerror(errno);

  // Wait on the semaphore. Should succeed immediately.
  ret = sem_wait(&sem);
  EXPECT_EQ(ret, 0) << "sem_wait failed: " << strerror(errno);

  // Post to the semaphore, incrementing its value.
  ret = sem_post(&sem);
  EXPECT_EQ(ret, 0) << "sem_post failed: " << strerror(errno);

  ret = sem_destroy(&sem);
  EXPECT_EQ(ret, 0) << "sem_destroy failed: " << strerror(errno);
}

// Tests that sem_timedwait succeeds immediately if the semaphore has a
// positive value.
TEST(PosixSemaphoreTest, TimedWaitSucceeds) {
  sem_t sem;
  // Initialize with a value of 1.
  int ret = sem_init(&sem, 0, 1);
  ASSERT_EQ(ret, 0) << "sem_init failed: " << strerror(errno);

  struct timespec ts;
  ret = clock_gettime(CLOCK_REALTIME, &ts);
  ASSERT_EQ(ret, 0) << "clock_gettime failed: " << strerror(errno);
  ts.tv_sec += 5;  // 5-second timeout, should not be needed.

  // This should succeed immediately without waiting.
  ret = sem_timedwait(&sem, &ts);
  EXPECT_EQ(ret, 0) << "sem_timedwait failed: " << strerror(errno);

  ret = sem_destroy(&sem);
  EXPECT_EQ(ret, 0) << "sem_destroy failed: " << strerror(errno);
}

// Tests that sem_timedwait correctly times out if the semaphore value is 0.
TEST(PosixSemaphoreTest, TimedWaitTimesOut) {
  sem_t sem;
  // Initialize with a value of 0, so any wait should block.
  int ret = sem_init(&sem, 0, 0);
  ASSERT_EQ(ret, 0) << "sem_init failed: " << strerror(errno);

  struct timespec ts;
  ret = clock_gettime(CLOCK_REALTIME, &ts);
  ASSERT_EQ(ret, 0) << "clock_gettime failed: " << strerror(errno);
  // Set a short timeout (100 milliseconds).
  ts.tv_nsec += 100 * 1000 * 1000;
  if (ts.tv_nsec >= 1000000000) {
    ts.tv_sec++;
    ts.tv_nsec -= 1000000000;
  }

  // Expect this call to block and then time out.
  ret = sem_timedwait(&sem, &ts);
  EXPECT_EQ(ret, -1) << "sem_timedwait did not fail as expected.";
  if (ret == -1) {
    EXPECT_EQ(errno, ETIMEDOUT)
        << "sem_timedwait failed with an unexpected error: " << strerror(errno);
  }

  ret = sem_destroy(&sem);
  EXPECT_EQ(ret, 0) << "sem_destroy failed: " << strerror(errno);
}

// Tests that a post operation correctly unblocks a subsequent wait.
TEST(PosixSemaphoreTest, PostThenWait) {
  sem_t sem;
  // Initialize with a value of 0.
  int ret = sem_init(&sem, 0, 0);
  ASSERT_EQ(ret, 0) << "sem_init failed: " << strerror(errno);

  // Post to the semaphore, making its value 1.
  ret = sem_post(&sem);
  EXPECT_EQ(ret, 0) << "sem_post failed: " << strerror(errno);

  // Now, wait on the semaphore. It should succeed immediately.
  ret = sem_wait(&sem);
  EXPECT_EQ(ret, 0) << "sem_wait after sem_post failed: " << strerror(errno);

  ret = sem_destroy(&sem);
  EXPECT_EQ(ret, 0) << "sem_destroy failed: " << strerror(errno);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

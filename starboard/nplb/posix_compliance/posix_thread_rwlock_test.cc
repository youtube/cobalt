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
#include <string.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixRwlockTest, BasicInitDestroy) {
  pthread_rwlock_t rwlock;
  int ret = pthread_rwlock_init(&rwlock, NULL);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_init failed: " << strerror(ret);

  if (ret == 0) {
    ret = pthread_rwlock_destroy(&rwlock);
    EXPECT_EQ(ret, 0) << "pthread_rwlock_destroy failed: " << strerror(ret);
  }
}

TEST(PosixRwlockTest, RdLockUnlock) {
  pthread_rwlock_t test_rwlock;
  int ret = pthread_rwlock_init(&test_rwlock, NULL);
  ASSERT_EQ(ret, 0) << "pthread_rwlock_init failed: " << strerror(ret);

  ret = pthread_rwlock_rdlock(&test_rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_rdlock failed: " << strerror(ret);

  if (ret == 0) {
    ret = pthread_rwlock_unlock(&test_rwlock);
    EXPECT_EQ(ret, 0) << "pthread_rwlock_unlock (after rdlock) failed: "
                      << strerror(ret);
  }

  ret = pthread_rwlock_destroy(&test_rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_destroy failed: " << strerror(ret);
}

TEST(PosixRwlockTest, WrLockUnlock) {
  pthread_rwlock_t rwlock;
  int ret = pthread_rwlock_init(&rwlock, NULL);
  ASSERT_EQ(ret, 0) << "pthread_rwlock_init failed: " << strerror(ret);

  ret = pthread_rwlock_wrlock(&rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_wrlock failed: " << strerror(ret);

  if (ret == 0) {
    ret = pthread_rwlock_unlock(&rwlock);
    EXPECT_EQ(ret, 0) << "pthread_rwlock_unlock (after wrlock) failed: "
                      << strerror(ret);
  }
  ret = pthread_rwlock_destroy(&rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_destroy failed: " << strerror(ret);
}

TEST(PosixRwlockTest, TryRdLockSuccess) {
  pthread_rwlock_t rwlock;
  int ret = pthread_rwlock_init(&rwlock, NULL);
  ASSERT_EQ(ret, 0) << "pthread_rwlock_init failed: " << strerror(ret);

  ret = pthread_rwlock_tryrdlock(&rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_tryrdlock failed: " << strerror(ret);

  if (ret == 0) {
    ret = pthread_rwlock_unlock(&rwlock);
    EXPECT_EQ(ret, 0) << "pthread_rwlock_unlock (after tryrdlock) failed: "
                      << strerror(ret);
  }
  ret = pthread_rwlock_destroy(&rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_destroy failed: " << strerror(ret);
}

TEST(PosixRwlockTest, TryWrLockSuccess) {
  pthread_rwlock_t rwlock;
  int ret = pthread_rwlock_init(&rwlock, NULL);
  ASSERT_EQ(ret, 0) << "pthread_rwlock_init failed: " << strerror(ret);

  ret = pthread_rwlock_trywrlock(&rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_trywrlock failed: " << strerror(ret);

  if (ret == 0) {
    ret = pthread_rwlock_unlock(&rwlock);
    EXPECT_EQ(ret, 0) << "pthread_rwlock_unlock (after trywrlock) failed: "
                      << strerror(ret);
  }
  ret = pthread_rwlock_destroy(&rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_destroy failed: " << strerror(ret);
}

TEST(PosixRwlockTest, TryRdLockFailsIfWriteLocked) {
  pthread_rwlock_t rwlock;
  int ret = pthread_rwlock_init(&rwlock, NULL);
  ASSERT_EQ(ret, 0) << "pthread_rwlock_init failed: " << strerror(ret);

  // Acquire write lock.
  ret = pthread_rwlock_wrlock(&rwlock);
  ASSERT_EQ(ret, 0) << "Setup: pthread_rwlock_wrlock failed: " << strerror(ret);

  // Attempt to try-read-lock.
  int try_ret = pthread_rwlock_tryrdlock(&rwlock);
  EXPECT_EQ(try_ret, EBUSY) << "pthread_rwlock_tryrdlock did not return EBUSY "
                               "when write-locked. Got: "
                            << strerror(try_ret) << " (" << try_ret << ")";

  // Unlock the write lock.
  ret = pthread_rwlock_unlock(&rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_unlock (after wrlock) failed: "
                    << strerror(ret);

  ret = pthread_rwlock_destroy(&rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_destroy failed: " << strerror(ret);
}

TEST(PosixRwlockTest, TryWrLockFailsIfReadLocked) {
  pthread_rwlock_t rwlock;
  int ret = pthread_rwlock_init(&rwlock, NULL);
  ASSERT_EQ(ret, 0) << "pthread_rwlock_init failed: " << strerror(ret);

  // Acquire read lock.
  ret = pthread_rwlock_rdlock(&rwlock);
  ASSERT_EQ(ret, 0) << "Setup: pthread_rwlock_rdlock failed: " << strerror(ret);

  // Attempt to try-write-lock.
  int try_ret = pthread_rwlock_trywrlock(&rwlock);
  EXPECT_EQ(try_ret, EBUSY)
      << "pthread_rwlock_trywrlock did not return EBUSY when read-locked. Got: "
      << strerror(try_ret) << " (" << try_ret << ")";

  // Unlock the read lock.
  ret = pthread_rwlock_unlock(&rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_unlock (after rdlock) failed: "
                    << strerror(ret);

  ret = pthread_rwlock_destroy(&rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_destroy failed: " << strerror(ret);
}

TEST(PosixRwlockTest, TryWrLockFailsIfWriteLocked) {
  pthread_rwlock_t rwlock;
  int ret = pthread_rwlock_init(&rwlock, NULL);
  ASSERT_EQ(ret, 0) << "pthread_rwlock_init failed: " << strerror(ret);

  // Acquire write lock.
  ret = pthread_rwlock_wrlock(&rwlock);
  ASSERT_EQ(ret, 0) << "Setup: pthread_rwlock_wrlock failed: " << strerror(ret);

  // Attempt to try-write-lock again.
  int try_ret = pthread_rwlock_trywrlock(&rwlock);
  EXPECT_EQ(try_ret, EBUSY) << "pthread_rwlock_trywrlock did not return EBUSY "
                               "when already write-locked. Got: "
                            << strerror(try_ret) << " (" << try_ret << ")";

  // Unlock the write lock.
  ret = pthread_rwlock_unlock(&rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_unlock (after wrlock) failed: "
                    << strerror(ret);

  ret = pthread_rwlock_destroy(&rwlock);
  EXPECT_EQ(ret, 0) << "pthread_rwlock_destroy failed: " << strerror(ret);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

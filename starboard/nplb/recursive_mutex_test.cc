// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/recursive_mutex.h"
#include "starboard/nplb/thread_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

TEST(RecursiveMutex, UnlockAndLockAgain) {
  RecursiveMutex rmutex;
  rmutex.Acquire();
  rmutex.Release();
  EXPECT_TRUE(rmutex.AcquireTry());
}

// RecursiveMutex is defined to be able to acquire lock as long as called on
// the same thread.
TEST(RecursiveMutex, LockTwice) {
  RecursiveMutex rmutex;
  rmutex.Acquire();
  EXPECT_TRUE(rmutex.AcquireTry());
}

TEST(RecursiveMutex, TryLockTwice) {
  RecursiveMutex rmutex;
  rmutex.AcquireTry();
  EXPECT_TRUE(rmutex.AcquireTry());
}

TEST(RecursiveMutex, TryLocksInLoop) {
  RecursiveMutex rmutex;
  for (unsigned int i = 0; i < 100; ++i) {
    EXPECT_TRUE(rmutex.AcquireTry());
  }
  for (unsigned int i = 0; i < 100; ++i) {
    rmutex.Release();
  }
  EXPECT_TRUE(rmutex.AcquireTry());
}

TEST(RecursiveMutex, LocksInLoop) {
  RecursiveMutex rmutex;
  for (unsigned int i = 0; i < 100; ++i) {
    rmutex.Acquire();
  }
  EXPECT_TRUE(rmutex.AcquireTry());
  for (unsigned int i = 0; i < 100; ++i) {
    rmutex.Release();
  }
  EXPECT_TRUE(rmutex.AcquireTry());
}

void RecursivelyLock(RecursiveMutex* rmutex, int level) {
  if (level == 100) {
    EXPECT_TRUE(rmutex->AcquireTry());
    return;
  }
  rmutex->Acquire();
  RecursivelyLock(rmutex, level + 1);
  rmutex->Release();
}

TEST(RecursiveMutex, TryLocksInRecursion) {
  RecursiveMutex rmutex;
  RecursivelyLock(&rmutex, 0);
  EXPECT_TRUE(rmutex.AcquireTry());
}

class ThreadBlockedRecursiveMutex : public AbstractTestThread {
 public:
  explicit ThreadBlockedRecursiveMutex(RecursiveMutex* s) : rmutex_(s) {}
  void Run() override { EXPECT_FALSE(rmutex_->AcquireTry()); }

 private:
  RecursiveMutex* rmutex_;
};

TEST(RecursiveMutex, BlockOtherThread) {
  RecursiveMutex rmutex;
  ThreadBlockedRecursiveMutex thread(&rmutex);
  rmutex.Acquire();
  thread.Start();
  EXPECT_TRUE(rmutex.AcquireTry());
  thread.Join();
  EXPECT_TRUE(rmutex.AcquireTry());
}

class ThreadAcquiresRecursiveMutex : public AbstractTestThread {
 public:
  explicit ThreadAcquiresRecursiveMutex(RecursiveMutex* s,
                                        Mutex* cv_mutex,
                                        ConditionVariable* tried_lock)
      : rmutex_(s), cv_mutex_(cv_mutex), tried_lock_(tried_lock) {}
  void Run() override {
    // This thread should not get the recursive mutex which is held by main
    // test thread.
    EXPECT_FALSE(rmutex_->AcquireTry());
    cv_mutex_->Acquire();
    tried_lock_->Signal();
    cv_mutex_->Release();
    rmutex_->Acquire();
    EXPECT_TRUE(rmutex_->AcquireTry());
    rmutex_->Release();
    rmutex_->Release();
  }

 private:
  RecursiveMutex* rmutex_;
  Mutex* cv_mutex_;
  ConditionVariable* tried_lock_;
};

TEST(RecursiveMutex, OtherThreadAcquires) {
  RecursiveMutex rmutex;
  Mutex cv_mutex;
  ScopedLock scoped_lock(cv_mutex);
  ConditionVariable other_thread_tried(cv_mutex);
  ThreadAcquiresRecursiveMutex thread(&rmutex, &cv_mutex, &other_thread_tried);
  rmutex.Acquire();
  EXPECT_TRUE(rmutex.AcquireTry());

  thread.Start();
  // Wait for the started thread to wait on the rmutex.
  other_thread_tried.Wait();

  EXPECT_TRUE(rmutex.AcquireTry());
  rmutex.Release();
  rmutex.Release();
  rmutex.Release();
  thread.Join();
}

}  // namespace nplb
}  // namespace starboard

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

#include <pthread.h>
#include <sys/types.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// Clobber the stack with non-zero values to ensure static initializers
// actually initialize the memory and don't just rely on the stack being zeroed.
__attribute__((noinline)) void ClobberStack() {
  volatile uint8_t buffer[1024];
  for (int i = 0; i < 1024; ++i) {
    buffer[i] = 0xAA;
  }
}

TEST(PosixStaticInitTest, PthreadMutexInitializer) {
  ClobberStack();
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  // Should be able to lock and unlock immediately without calling
  // pthread_mutex_init.
  EXPECT_EQ(pthread_mutex_lock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_trylock(&mutex), EBUSY);
  EXPECT_EQ(pthread_mutex_unlock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_destroy(&mutex), 0);
}

TEST(PosixStaticInitTest, PthreadRecursiveMutexInitializer) {
  ClobberStack();
  pthread_mutex_t mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
  // Should be able to lock multiple times immediately without calling
  // pthread_mutex_init.
  EXPECT_EQ(pthread_mutex_lock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_lock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_trylock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_unlock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_unlock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_unlock(&mutex), 0);
  EXPECT_EQ(pthread_mutex_destroy(&mutex), 0);
}

TEST(PosixStaticInitTest, PthreadRwlockInitializer) {
  ClobberStack();
  pthread_rwlock_t rwlock = PTHREAD_RWLOCK_INITIALIZER;
  // Should be able to lock and unlock immediately without calling
  // pthread_rwlock_init.
  EXPECT_EQ(pthread_rwlock_rdlock(&rwlock), 0);
  EXPECT_EQ(pthread_rwlock_unlock(&rwlock), 0);
  EXPECT_EQ(pthread_rwlock_wrlock(&rwlock), 0);
  EXPECT_EQ(pthread_rwlock_unlock(&rwlock), 0);
  EXPECT_EQ(pthread_rwlock_destroy(&rwlock), 0);
}

TEST(PosixStaticInitTest, PthreadCondInitializer) {
  ClobberStack();
  pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
  // Should be able to signal and broadcast immediately without calling
  // pthread_cond_init.
  EXPECT_EQ(pthread_cond_signal(&cond), 0);
  EXPECT_EQ(pthread_cond_broadcast(&cond), 0);
  EXPECT_EQ(pthread_cond_destroy(&cond), 0);
}

int g_once_called = 0;
void OnceInitRoutine() {
  g_once_called++;
}

TEST(PosixStaticInitTest, PthreadOnceInit) {
  ClobberStack();
  pthread_once_t once_control = PTHREAD_ONCE_INIT;
  g_once_called = 0;
  EXPECT_EQ(pthread_once(&once_control, OnceInitRoutine), 0);
  EXPECT_EQ(g_once_called, 1);
  EXPECT_EQ(pthread_once(&once_control, OnceInitRoutine), 0);
  EXPECT_EQ(g_once_called, 1);
}

}  // namespace
}  // namespace nplb

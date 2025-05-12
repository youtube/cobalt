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

#include <pthread.h>

#include "testing/gtest/include/gtest/gtest.h"
namespace starboard {
namespace nplb {
namespace {

TEST(PosixMutexAttrTest, MutexAttrInitDestroy) {
  pthread_mutexattr_t mattr;
  int result = pthread_mutexattr_init(&mattr);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_init failed: " << strerror(result);

  result = pthread_mutexattr_destroy(&mattr);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_destroy failed: "
                       << strerror(result);
}

TEST(PosixMutexAttrTest, MutexAttrType) {
  pthread_mutexattr_t mattr;
  int result = pthread_mutexattr_init(&mattr);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_init failed: " << strerror(result);

  int type = -1;

  // Get the default mutex type.
  result = pthread_mutexattr_gettype(&mattr, &type);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_gettype failed: "
                       << strerror(result);
  EXPECT_EQ(type, PTHREAD_MUTEX_DEFAULT);

  // Set mutex type to PTHREAD_MUTEX_RECURSIVE.
  result = pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_RECURSIVE);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_settype (RECURSIVE) failed: "
                       << strerror(result);
  result = pthread_mutexattr_gettype(&mattr, &type);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_gettype failed: "
                       << strerror(result);
  EXPECT_EQ(type, PTHREAD_MUTEX_RECURSIVE);

  // Set mutex type to PTHREAD_MUTEX_ERRORCHECK.
  result = pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_ERRORCHECK);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_settype (ERRORCHECK) failed: "
                       << strerror(result);
  result = pthread_mutexattr_gettype(&mattr, &type);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_gettype failed: "
                       << strerror(result);
  EXPECT_EQ(type, PTHREAD_MUTEX_ERRORCHECK);

  // Set mutex type to PTHREAD_MUTEX_NORMAL.
  result = pthread_mutexattr_settype(&mattr, PTHREAD_MUTEX_NORMAL);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_settype (NORMAL) failed: "
                       << strerror(result);
  result = pthread_mutexattr_gettype(&mattr, &type);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_gettype failed: "
                       << strerror(result);
  EXPECT_EQ(type, PTHREAD_MUTEX_NORMAL);

  result = pthread_mutexattr_destroy(&mattr);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_destroy failed: "
                       << strerror(result);
}

TEST(PosixMutexAttrTest, MutexAttrPshared) {
  pthread_mutexattr_t mattr;
  int result = pthread_mutexattr_init(&mattr);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_init failed: " << strerror(result);

  int pshared = 0;

  // Get the default pshared state.
  result = pthread_mutexattr_getpshared(&mattr, &pshared);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_getpshared failed: "
                       << strerror(result);
  EXPECT_EQ(pshared, PTHREAD_PROCESS_PRIVATE);

  // Set pshared state to PTHREAD_PROCESS_SHARED.
  result = pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_setpshared failed: "
                       << strerror(result);
  result = pthread_mutexattr_getpshared(&mattr, &pshared);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_getpshared failed: "
                       << strerror(result);
  EXPECT_EQ(pshared, PTHREAD_PROCESS_SHARED);

  // Set pshared state back to PTHREAD_PROCESS_PRIVATE.
  result = pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_PRIVATE);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_setpshared (PRIVATE) failed: "
                       << strerror(result);
  result = pthread_mutexattr_getpshared(&mattr, &pshared);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_getpshared failed: "
                       << strerror(result);
  EXPECT_EQ(pshared, PTHREAD_PROCESS_PRIVATE);

  result = pthread_mutexattr_destroy(&mattr);
  EXPECT_EQ(result, 0) << "pthread_mutexattr_destroy failed: "
                       << strerror(result);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

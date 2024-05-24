// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

constexpr int kStackSize = 4 * 1024 * 1024;

TEST(PosixThreadAttrTest, InitAttr) {
  pthread_attr_t attr;
  EXPECT_EQ(pthread_attr_init(&attr), 0);
  EXPECT_EQ(pthread_attr_destroy(&attr), 0);
}

TEST(PosixThreadAttrTest, DetachAttr) {
  pthread_attr_t attr;
  EXPECT_EQ(pthread_attr_init(&attr), 0);
  int detach_state = PTHREAD_CREATE_JOINABLE;

  EXPECT_EQ(pthread_attr_getdetachstate(&attr, &detach_state), 0);
  EXPECT_EQ(detach_state, PTHREAD_CREATE_JOINABLE);

  EXPECT_EQ(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED), 0);

  EXPECT_EQ(pthread_attr_getdetachstate(&attr, &detach_state), 0);
  EXPECT_EQ(detach_state, PTHREAD_CREATE_DETACHED);

  EXPECT_EQ(pthread_attr_destroy(&attr), 0);
}

TEST(PosixThreadAttrTest, StackSizeAttr) {
  pthread_attr_t attr;
  EXPECT_EQ(pthread_attr_init(&attr), 0);
  size_t stack_size = 0;

  EXPECT_EQ(pthread_attr_setstacksize(&attr, kStackSize), 0);

  EXPECT_EQ(pthread_attr_getstacksize(&attr, &stack_size), 0);
  EXPECT_EQ(stack_size, kStackSize);

  EXPECT_EQ(pthread_attr_destroy(&attr), 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

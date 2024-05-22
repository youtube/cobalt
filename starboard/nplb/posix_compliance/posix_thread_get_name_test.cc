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

#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void* GetThreadNameEntryPoint(void* context) {
  pthread_setname_np(pthread_self(), posix::kThreadName);

  char name[4096] = {0};
  pthread_getname_np(pthread_self(), name, SB_ARRAY_SIZE_INT(name));
  std::string* result = static_cast<std::string*>(context);
  *result = name;
  return NULL;
}

TEST(PosixThreadGetNameTest, SunnyDay) {
  std::string result;

  pthread_t thread;
  EXPECT_EQ(pthread_create(&thread, NULL, GetThreadNameEntryPoint, &result), 0);

  EXPECT_TRUE(thread != 0);
  EXPECT_EQ(pthread_join(thread, NULL), 0);
  EXPECT_EQ(posix::kThreadName, result);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

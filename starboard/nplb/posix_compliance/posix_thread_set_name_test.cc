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
#include <sys/prctl.h>

#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

struct Context {
  std::string got_name1;
  std::string name_to_set;
  std::string got_name2;
};

// Gets the thread's name and sets it in the context.
void* SetThreadNameEntryPoint(void* context) {
  char name[4096] = {0};
  Context* real_context = static_cast<Context*>(context);

#if __ANDROID_API__ < 26
  prctl(PR_GET_NAME, name, 0L, 0L, 0L);
#else
  pthread_getname_np(pthread_self(), name, SB_ARRAY_SIZE_INT(name));
#endif  // __ANDROID_API__ < 26
  real_context->got_name1 = name;

  pthread_setname_np(pthread_self(), real_context->name_to_set.c_str());

#if __ANDROID_API__ < 26
  prctl(PR_GET_NAME, name, 0L, 0L, 0L);
#else
  pthread_getname_np(pthread_self(), name, SB_ARRAY_SIZE_INT(name));
#endif  // __ANDROID_API__ < 26
  real_context->got_name2 = name;

  return NULL;
}

TEST(PosixThreadSetNameTest, SunnyDay) {
  Context context;
  context.name_to_set = posix::kAltThreadName;
  pthread_t thread;
  EXPECT_EQ(pthread_create(&thread, NULL, SetThreadNameEntryPoint, &context),
            0);
  EXPECT_TRUE(thread != 0);
  EXPECT_EQ(pthread_join(thread, NULL), 0);
  EXPECT_NE(posix::kAltThreadName, context.got_name1);
  EXPECT_EQ(posix::kAltThreadName, context.got_name2);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

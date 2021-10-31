// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Thread joining is mostly tested in the other tests.

#include "starboard/nplb/thread_helpers.h"
#include "starboard/thread.h"
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

  SbThreadGetName(name, SB_ARRAY_SIZE_INT(name));
  real_context->got_name1 = name;

  SbThreadSetName(real_context->name_to_set.c_str());

  SbThreadGetName(name, SB_ARRAY_SIZE_INT(name));
  real_context->got_name2 = name;

  return NULL;
}

TEST(SbThreadSetNameTest, SunnyDay) {
  Context context;
  context.name_to_set = kAltThreadName;
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                     kThreadName, SetThreadNameEntryPoint, &context);
  EXPECT_TRUE(SbThreadIsValid(thread));
  EXPECT_TRUE(SbThreadJoin(thread, NULL));
  EXPECT_EQ(kThreadName, context.got_name1);
  EXPECT_EQ(kAltThreadName, context.got_name2);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

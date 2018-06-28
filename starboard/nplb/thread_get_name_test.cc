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

// Returns the thread's ID.
void* GetThreadNameEntryPoint(void* context) {
  char name[4096] = {0};
  SbThreadGetName(name, SB_ARRAY_SIZE_INT(name));
  std::string* result = static_cast<std::string*>(context);
  *result = name;
  return NULL;
}

TEST(SbThreadGetNameTest, SunnyDay) {
  std::string result;
  SbThread thread =
      SbThreadCreate(0, kSbThreadNoPriority, kSbThreadNoAffinity, true,
                     kThreadName, GetThreadNameEntryPoint, &result);
  EXPECT_TRUE(SbThreadIsValid(thread));
  EXPECT_TRUE(SbThreadJoin(thread, NULL));
  EXPECT_EQ(kThreadName, result);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

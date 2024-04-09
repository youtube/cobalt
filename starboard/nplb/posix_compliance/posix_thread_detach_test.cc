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

// Thread joining is mostly tested in the other tests.

#include "starboard/nplb/posix_compliance/posix_thread_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixThreadDetachTest, SunnyDay) {
  pthread_t thread;
  EXPECT_EQ(pthread_create(&thread, NULL, posix::AddOneEntryPoint, NULL), 0);

  EXPECT_EQ(pthread_detach(thread), 0);
  void* result = NULL;
  EXPECT_NE(pthread_join(thread, &result), 0);
  EXPECT_EQ(NULL, result);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

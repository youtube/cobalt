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

// flush() is otherwise tested in PosixFileWriteTest.

#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixFileFsyncTest, InvalidFileErrors) {
  const int result = fsync(-1);
  EXPECT_NE(result, 0) << "fsync with invalid file descriptor should fail";
  EXPECT_EQ(errno, EBADF) << "fsync with invalid file descriptor should set "
                             "errno to EBADF, but got "
                          << strerror(errno);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

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

#include <fcntl.h>

#include <string>

#include "starboard/common/file_wrapper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(PosixFileModeStringToFlagsTest, Empties) {
  EXPECT_EQ(0, FileModeStringToFlags(NULL));
  EXPECT_EQ(0, FileModeStringToFlags(""));
}

TEST(PosixFileModeStringToFlagsTest, ReadMode) {
  EXPECT_EQ(O_RDONLY, FileModeStringToFlags("r"));
  EXPECT_EQ(O_RDWR, FileModeStringToFlags("r+"));
  EXPECT_EQ(O_RDWR, FileModeStringToFlags("r+b"));
  EXPECT_EQ(O_RDWR, FileModeStringToFlags("rb+"));
}

TEST(PosixFileModeStringToFlagsTest, WriteMode) {
  EXPECT_EQ(O_CREAT | O_TRUNC | O_WRONLY, FileModeStringToFlags("w"));
  EXPECT_EQ(O_CREAT | O_TRUNC | O_RDWR, FileModeStringToFlags("w+"));
  EXPECT_EQ(O_CREAT | O_TRUNC | O_RDWR, FileModeStringToFlags("w+b"));
  EXPECT_EQ(O_CREAT | O_TRUNC | O_RDWR, FileModeStringToFlags("wb+"));
}

TEST(PosixFileModeStringToFlagsTest, AppendMode) {
  EXPECT_EQ(O_CREAT | O_WRONLY, FileModeStringToFlags("a"));
  EXPECT_EQ(O_CREAT | O_RDWR, FileModeStringToFlags("a+"));
  EXPECT_EQ(O_CREAT | O_RDWR, FileModeStringToFlags("a+b"));
  EXPECT_EQ(O_CREAT | O_RDWR, FileModeStringToFlags("ab+"));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

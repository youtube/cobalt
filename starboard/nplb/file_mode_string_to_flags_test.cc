// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <string>

#include "starboard/file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbFileModeStringToFlagsTest, Empties) {
  EXPECT_EQ(0, SbFileModeStringToFlags(NULL));
  EXPECT_EQ(0, SbFileModeStringToFlags(""));
}

TEST(SbFileModeStringToFlagsTest, Arr) {
  EXPECT_EQ(kSbFileOpenOnly | kSbFileRead, SbFileModeStringToFlags("r"));
  EXPECT_EQ(kSbFileOpenOnly | kSbFileRead | kSbFileWrite,
            SbFileModeStringToFlags("r+"));
  EXPECT_EQ(kSbFileOpenOnly | kSbFileRead | kSbFileWrite,
            SbFileModeStringToFlags("r+b"));
  EXPECT_EQ(kSbFileOpenOnly | kSbFileRead | kSbFileWrite,
            SbFileModeStringToFlags("rb+"));
}

TEST(SbFileModeStringToFlagsTest, Wuh) {
  EXPECT_EQ(kSbFileCreateAlways | kSbFileWrite, SbFileModeStringToFlags("w"));
  EXPECT_EQ(kSbFileCreateAlways | kSbFileWrite | kSbFileRead,
            SbFileModeStringToFlags("w+"));
  EXPECT_EQ(kSbFileCreateAlways | kSbFileWrite | kSbFileRead,
            SbFileModeStringToFlags("w+b"));
  EXPECT_EQ(kSbFileCreateAlways | kSbFileWrite | kSbFileRead,
            SbFileModeStringToFlags("wb+"));
}

TEST(SbFileModeStringToFlagsTest, Aah) {
  EXPECT_EQ(kSbFileOpenAlways | kSbFileWrite, SbFileModeStringToFlags("a"));
  EXPECT_EQ(kSbFileOpenAlways | kSbFileWrite | kSbFileRead,
            SbFileModeStringToFlags("a+"));
  EXPECT_EQ(kSbFileOpenAlways | kSbFileWrite | kSbFileRead,
            SbFileModeStringToFlags("a+b"));
  EXPECT_EQ(kSbFileOpenAlways | kSbFileWrite | kSbFileRead,
            SbFileModeStringToFlags("ab+"));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

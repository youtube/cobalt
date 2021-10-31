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

// This test doesn't test that the appropriate output was produced, but ensures
// it compiles and runs without crashing.

#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbLogRawTest, SunnyDayEmpty) {
  SbLogRaw("");
}

TEST(SbLogRawTest, SunnyDayNewline) {
  SbLogRaw("\n");
}

TEST(SbLogRawTest, SunnyDayTextNoNewline) {
  SbLogRaw("test");
  // Add the newline separately to avoid messing up the output.
  SbLogRaw("\n");
}

TEST(SbLogRawTest, SunnyDayTextAndNewline) {
  SbLogRaw("test\n");
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

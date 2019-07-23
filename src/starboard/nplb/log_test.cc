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

TEST(SbLogTest, SunnyDayEmpty) {
  SbLog(kSbLogPriorityInfo, "");
}

TEST(SbLogTest, SunnyDayNewline) {
  SbLog(kSbLogPriorityInfo, "\n");
}

TEST(SbLogTest, SunnyDayTextNoNewline) {
  SbLog(kSbLogPriorityInfo, "test");

  // Add a newline so the test output doesn't get too messed up.
  SbLog(kSbLogPriorityInfo, "\n");
}

TEST(SbLogTest, SunnyDayTextAndNewline) {
  SbLog(kSbLogPriorityInfo, "test\n");
}

TEST(SbLogTest, SunnyDayWarn) {
  SbLog(kSbLogPriorityWarning, "warning\n");
}

TEST(SbLogTest, SunnyDayError) {
  SbLog(kSbLogPriorityError, "erroring\n");
}

TEST(SbLogTest, SunnyDayFatal) {
  SbLog(kSbLogPriorityFatal, "fataling\n");
}

TEST(SbLogTest, SunnyDayStreams) {
  SbLogPriority previous = logging::GetMinLogLevel();
  for (int i = kSbLogPriorityInfo; i <= kSbLogPriorityFatal; ++i) {
    logging::SetMinLogLevel(static_cast<SbLogPriority>(i));
    EXPECT_EQ(i, logging::GetMinLogLevel());
    SB_LOG(INFO) << "testing info";
    SB_DLOG(INFO) << "testing debug info";
    SB_LOG(WARNING) << "testing warning";
    SB_DLOG(WARNING) << "testing debug warning";
    SB_LOG(ERROR) << "testing error";
    SB_DLOG(ERROR) << "testing debug error";
  }

  logging::SetMinLogLevel(previous);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

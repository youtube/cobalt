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

#include "cobalt/watchdog//instrumentation_log.h"

#include <set>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "starboard/common/file.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace watchdog {

class InstrumentationLogTest : public testing::Test {
 protected:
  InstrumentationLog* instrumentation_log_;
};

TEST_F(InstrumentationLogTest, CanCallLogEvent) {
  InstrumentationLog log;
  log.LogEvent("abc");

  ASSERT_EQ(log.GetLogTrace().size(), 1);
}

TEST_F(InstrumentationLogTest, CapsTheBuffer) {
  InstrumentationLog log;

  for (int i = 0; i < kBufferSize; i++) {
    log.LogEvent(std::to_string(i));
  }

  log.LogEvent("1");
  log.LogEvent("2");

  ASSERT_EQ(log.GetLogTrace().size(), kBufferSize);
}

TEST_F(InstrumentationLogTest, LogEventReturnsFalseOnMaxLenExceed) {
  InstrumentationLog log;

  std::string maxLenEvent(kMaxEventLen, 'a');
  ASSERT_TRUE(log.LogEvent(maxLenEvent));

  std::string exceedingLenEvent(kMaxEventLen + 1, 'a');
  ASSERT_FALSE(log.LogEvent(exceedingLenEvent));
}

TEST_F(InstrumentationLogTest, GetLogTraceReturnsEventsInOrder) {
  InstrumentationLog log;

  for (int i = 0; i < kBufferSize; i++) {
    log.LogEvent(std::to_string(i));
  }

  log.LogEvent("1");
  log.LogEvent("2");
  log.LogEvent("3");
  log.LogEvent("4");
  log.LogEvent("5");

  ASSERT_EQ(log.GetLogTrace().at(kBufferSize - 1 - 4), "1");
  ASSERT_EQ(log.GetLogTrace().at(kBufferSize - 1 - 3), "2");
  ASSERT_EQ(log.GetLogTrace().at(kBufferSize - 1 - 2), "3");
  ASSERT_EQ(log.GetLogTrace().at(kBufferSize - 1 - 1), "4");
  ASSERT_EQ(log.GetLogTrace().at(kBufferSize - 1), "5");
}

TEST_F(InstrumentationLogTest, CanGetEmptyTraceAsValue) {
  InstrumentationLog log;

  ASSERT_EQ(log.GetLogTraceAsValue().GetList().size(), 0);
}

TEST_F(InstrumentationLogTest, CanGetLogTraceAsValue) {
  InstrumentationLog log;

  log.LogEvent("1");
  log.LogEvent("2");
  log.LogEvent("3");

  ASSERT_EQ(log.GetLogTraceAsValue().GetList().at(0).GetString(), "1");
  ASSERT_EQ(log.GetLogTraceAsValue().GetList().at(1).GetString(), "2");
  ASSERT_EQ(log.GetLogTraceAsValue().GetList().at(2).GetString(), "3");
}

TEST_F(InstrumentationLogTest, CanClearLog) {
  InstrumentationLog log;

  log.LogEvent("1");
  log.LogEvent("2");
  log.LogEvent("3");

  ASSERT_EQ(log.GetLogTrace().size(), 3);

  log.ClearLog();
  ASSERT_EQ(log.GetLogTrace().size(), 0);
}

TEST_F(InstrumentationLogTest, CanClearEmptyLog) {
  InstrumentationLog log;
  ASSERT_EQ(log.GetLogTrace().size(), 0);

  log.ClearLog();
  ASSERT_EQ(log.GetLogTrace().size(), 0);
}

}  // namespace watchdog
}  // namespace cobalt

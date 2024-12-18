// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard/common/log.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace logging {

static const std::map<SbLogPriority, std::string> kSbLogPriorityToString = {
    {kSbLogPriorityUnknown, "unknown"},
    {kSbLogPriorityInfo, "info"},
    {kSbLogPriorityWarning, "warning"},
    {kSbLogPriorityError, "error"},
    {kSbLogPriorityFatal, "fatal"}};

struct TextAndSbPriority {
  const std::string text;
  const SbLogPriority sb_log_priority;
};

using MinLogLevelTranslationTest = testing::TestWithParam<TextAndSbPriority>;

TEST_P(MinLogLevelTranslationTest, ExpectedBehaviour) {
  EXPECT_EQ(StringToLogLevel(GetParam().text), GetParam().sb_log_priority);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MinLogLevelTranslationTest,
    testing::ValuesIn(std::vector<struct TextAndSbPriority>{
        {"", SB_LOG_INFO},
        {"blabla", SB_LOG_INFO},
        {"info", SB_LOG_INFO},
        {"warning", SB_LOG_WARNING},
        {"error", SB_LOG_ERROR},
        {"fatal", SB_LOG_FATAL},
    }),
    [](const testing::TestParamInfo<MinLogLevelTranslationTest::ParamType>&
           info) {
      return "from_" + info.param.text + "_to_" +
             kSbLogPriorityToString.at(info.param.sb_log_priority);
    });

using ChromiumLogLevelTranslationTest =
    testing::TestWithParam<TextAndSbPriority>;

TEST_P(ChromiumLogLevelTranslationTest, ExpectedBehaviour) {
  EXPECT_EQ(ChromiumIntToStarboardLogLevel(GetParam().text),
            GetParam().sb_log_priority);
}

INSTANTIATE_TEST_SUITE_P(
    ,
    ChromiumLogLevelTranslationTest,
    testing::ValuesIn(std::vector<struct TextAndSbPriority>{
        {"0", SB_LOG_INFO},
        {"1", SB_LOG_INFO},
        {"2", SB_LOG_WARNING},
        {"3", SB_LOG_ERROR},
        {"4", SB_LOG_FATAL},
        {"5", SB_LOG_INFO},
        {"100", SB_LOG_INFO},
    }),
    [](const testing::TestParamInfo<ChromiumLogLevelTranslationTest::ParamType>&
           info) {
      return "from_" + info.param.text + "_to_" +
             kSbLogPriorityToString.at(info.param.sb_log_priority);
    });
}  // namespace logging
}  // namespace starboard

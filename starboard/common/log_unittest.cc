// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard/common/log.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

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

TEST(LogTest, StreamOperator_OptionalValue) {
  std::optional<int> present_value = 123;
  std::stringstream ss_present;
  ss_present << present_value;

  EXPECT_EQ(ss_present.str(), "123");
}

TEST(LogTest, StreamOperator_OptionalEmpty) {
  std::optional<int> absent_value;
  std::stringstream ss_absent;
  ss_absent << absent_value;

  EXPECT_EQ(ss_absent.str(), "(nullopt)");
}

struct CapturedSbLog {
  SbLogPriority priority = kSbLogPriorityUnknown;
  std::string file;
  int line = 0;
  std::string message;
  int calls = 0;
};

CapturedSbLog* g_captured_sb_log = nullptr;

bool CaptureSbLogHandler(SbLogPriority priority,
                         const char* file,
                         int line,
                         size_t message_start,
                         const std::string& str) {
  if (g_captured_sb_log) {
    g_captured_sb_log->priority = priority;
    g_captured_sb_log->file = file ? file : "";
    g_captured_sb_log->line = line;
    g_captured_sb_log->message =
        message_start <= str.size() ? str.substr(message_start) : str;
    ++g_captured_sb_log->calls;
  }
  return false;
}

// Verifies that registering an internal Starboard log handler captures SB_LOG
// messages with priority, source file/line, and payload offset.
TEST(LogTest, LogMessageHandlerInterceptsSbLog) {
  LogMessageHandlerFunction saved_handler = GetLogMessageHandler();
  CapturedSbLog captured;
  g_captured_sb_log = &captured;
  SetLogMessageHandler(&CaptureSbLogHandler);
  EXPECT_EQ(GetLogMessageHandler(), &CaptureSbLogHandler);

  SB_LOG(WARNING) << "starboard-log-handler-test";

  SetLogMessageHandler(saved_handler);
  g_captured_sb_log = nullptr;

  EXPECT_GE(captured.calls, 1);
  EXPECT_EQ(captured.priority, kSbLogPriorityWarning);
  EXPECT_NE(captured.file.find("log_unittest.cc"), std::string::npos);
  EXPECT_GT(captured.line, 0);
  EXPECT_NE(captured.message.find("starboard-log-handler-test"),
            std::string::npos);
}

}  // namespace
}  // namespace starboard

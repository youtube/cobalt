// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard/common/log.h"

#include <mutex>

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

constexpr char kSbLogHandlerTestSentinel[] = "starboard-log-handler-test";

struct CapturedSbLogState {
  std::mutex mutex;
  SbLogPriority priority = kSbLogPriorityUnknown;
  std::string file;
  int line = 0;
  std::string message;
  int calls = 0;
};

CapturedSbLogState& GetCapturedSbLogState() {
  static auto* state = new CapturedSbLogState();
  return *state;
}

class ScopedSbLogHandlerRestorer {
 public:
  explicit ScopedSbLogHandlerRestorer(LogMessageHandlerFunction saved_handler)
      : saved_handler_(saved_handler) {}
  ~ScopedSbLogHandlerRestorer() { SetLogMessageHandler(saved_handler_); }

 private:
  LogMessageHandlerFunction saved_handler_;
};

bool CaptureSbLogHandler(SbLogPriority priority,
                         const char* file,
                         int line,
                         size_t message_start,
                         const std::string& str) {
  std::string payload =
      message_start <= str.size() ? str.substr(message_start) : std::string();
  if (payload.find(kSbLogHandlerTestSentinel) == std::string::npos) {
    return false;
  }
  CapturedSbLogState& state = GetCapturedSbLogState();
  std::lock_guard<std::mutex> lock(state.mutex);
  state.priority = priority;
  state.file = file ? file : "";
  state.line = line;
  state.message = std::move(payload);
  ++state.calls;
  return false;
}

// Verifies that registering an internal Starboard log handler captures SB_LOG
// messages with exact priority, source file/line, and payload offset past the
// formatted header prefix.
TEST(LogTest, LogMessageHandlerInterceptsSbLog) {
  CapturedSbLogState& state = GetCapturedSbLogState();
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    state.priority = kSbLogPriorityUnknown;
    state.file.clear();
    state.line = 0;
    state.message.clear();
    state.calls = 0;
  }

  ScopedSbLogHandlerRestorer restorer(GetLogMessageHandler());
  SetLogMessageHandler(&CaptureSbLogHandler);
  EXPECT_EQ(GetLogMessageHandler(), &CaptureSbLogHandler);

  const int expected_line = __LINE__ + 1;
  SB_LOG(WARNING) << kSbLogHandlerTestSentinel;

  std::lock_guard<std::mutex> lock(state.mutex);
  EXPECT_EQ(state.calls, 1);
  EXPECT_EQ(state.priority, kSbLogPriorityWarning);
  EXPECT_NE(state.file.find("log_unittest.cc"), std::string::npos);
  EXPECT_EQ(state.line, expected_line);
  EXPECT_EQ(state.message, std::string(kSbLogHandlerTestSentinel) + "\n");
}

}  // namespace
}  // namespace starboard

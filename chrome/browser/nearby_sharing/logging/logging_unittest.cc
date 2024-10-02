// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "chrome/browser/nearby_sharing/logging/log_buffer.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"

namespace {

const char kLog1[] = "Mahogony destined to make a sturdy table";
const char kLog2[] = "Construction grade cedar";
const char kLog3[] = "Pine infested by hungry beetles";
const char kLog4[] = "Unremarkable maple";

// Called for every log message added to the standard logging system. The new
// log is saved in |standard_logs| and consumed so it does not flood stdout.
std::vector<std::string>& GetStandardLogs() {
  static base::NoDestructor<std::vector<std::string>> standard_logs;
  return *standard_logs;
}

bool HandleStandardLogMessage(int severity,
                              const char* file,
                              int line,
                              size_t message_start,
                              const std::string& str) {
  GetStandardLogs().push_back(str);
  return true;
}

}  // namespace

class NearbyShareInternalsLoggingTest : public testing::Test {
 public:
  NearbyShareInternalsLoggingTest() = default;

  void SetUp() override {
    LogBuffer::GetInstance()->Clear();
    GetStandardLogs().clear();

    previous_handler_ = logging::GetLogMessageHandler();
    logging::SetLogMessageHandler(&HandleStandardLogMessage);
  }

  void TearDown() override { logging::SetLogMessageHandler(previous_handler_); }

 private:
  logging::LogMessageHandlerFunction previous_handler_{nullptr};
};

TEST_F(NearbyShareInternalsLoggingTest, LogsSavedToBuffer) {
  int base_line_number = __LINE__;
  NS_LOG(INFO) << kLog1;
  NS_LOG(WARNING) << kLog2;
  NS_LOG(ERROR) << kLog3;
  NS_LOG(VERBOSE) << kLog3;

  auto* logs = LogBuffer::GetInstance()->logs();
  ASSERT_EQ(4u, logs->size());

  auto iterator = logs->begin();
  const LogBuffer::LogMessage& log_message1 = *iterator;
  EXPECT_EQ(kLog1, log_message1.text);
  EXPECT_EQ(__FILE__, log_message1.file);
  EXPECT_EQ(base_line_number + 1, log_message1.line);
  EXPECT_EQ(logging::LOG_INFO, log_message1.severity);

  ++iterator;
  const LogBuffer::LogMessage& log_message2 = *iterator;
  EXPECT_EQ(kLog2, log_message2.text);
  EXPECT_EQ(__FILE__, log_message2.file);
  EXPECT_EQ(base_line_number + 2, log_message2.line);
  EXPECT_EQ(logging::LOG_WARNING, log_message2.severity);

  ++iterator;
  const LogBuffer::LogMessage& log_message3 = *iterator;
  EXPECT_EQ(kLog3, log_message3.text);
  EXPECT_EQ(__FILE__, log_message3.file);
  EXPECT_EQ(base_line_number + 3, log_message3.line);
  EXPECT_EQ(logging::LOG_ERROR, log_message3.severity);

  ++iterator;
  const LogBuffer::LogMessage& log_message4 = *iterator;
  EXPECT_EQ(kLog3, log_message4.text);
  EXPECT_EQ(__FILE__, log_message4.file);
  EXPECT_EQ(base_line_number + 4, log_message4.line);
  EXPECT_EQ(logging::LOG_VERBOSE, log_message4.severity);
}

TEST_F(NearbyShareInternalsLoggingTest, LogWhenBufferIsFull) {
  EXPECT_EQ(0u, LogBuffer::GetInstance()->logs()->size());

  for (size_t i = 0; i < LogBuffer::GetInstance()->MaxBufferSize(); ++i) {
    NS_LOG(INFO) << "log " << i;
  }

  EXPECT_EQ(LogBuffer::GetInstance()->MaxBufferSize(),
            LogBuffer::GetInstance()->logs()->size());
  NS_LOG(INFO) << kLog1;
  EXPECT_EQ(LogBuffer::GetInstance()->MaxBufferSize(),
            LogBuffer::GetInstance()->logs()->size());

  auto iterator = LogBuffer::GetInstance()->logs()->begin();
  for (size_t i = 0; i < LogBuffer::GetInstance()->MaxBufferSize() - 1;
       ++iterator, ++i) {
    std::string expected_text = "log " + base::NumberToString(i + 1);
    EXPECT_EQ(expected_text, (*iterator).text);
  }
}

TEST_F(NearbyShareInternalsLoggingTest, StandardLogsCreated) {
  NS_LOG(INFO) << kLog1;
  NS_LOG(WARNING) << kLog2;
  NS_LOG(ERROR) << kLog3;
  NS_LOG(VERBOSE) << kLog4;

  ASSERT_EQ(3u, GetStandardLogs().size());
  EXPECT_NE(std::string::npos, GetStandardLogs()[0].find(kLog1));
  EXPECT_NE(std::string::npos, GetStandardLogs()[1].find(kLog2));
  EXPECT_NE(std::string::npos, GetStandardLogs()[2].find(kLog3));
}

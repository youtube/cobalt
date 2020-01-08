// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "base/log_once_unittest_1.h"
#include "base/log_once_unittest_2.h"
#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
static inline void log_once_unittest() {
  // Note: The LOG_ONCE statement below must remain on the same line as the
  // LOG_ONCE statements in log_once_unittest_1.h and log_once_unittest_2.h
  LOG_ONCE(INFO) << "This is log_once_unittest().";
  SB_DCHECK(__LINE__ == 28);  // Note keep in sync with the other statements.
}

static inline void notimplemented_log_once_unittest() {
  // Note: The NOTIMPLEMENTED_LOG_ONCE statement below must remain on the same
  // line as the NOTIMPLEMENTED_LOG_ONCE statements in log_once_unittest_1.h
  // and log_once_unittest_2.h
  NOTIMPLEMENTED_LOG_ONCE() << "This is notimplemented_log_once_unittest().";
  SB_DCHECK(__LINE__ == 36);  // Note keep in sync with the other statements.
}
}  // namespace

namespace logging {
namespace {

TEST(LogOnceTest, LogOnce) {
  // Set up a callback function to capture the log output string.
  auto old_log_message_handler = GetLogMessageHandler();
  // Use a static because only captureless lambdas can be converted to a
  // function pointer for SetLogMessageHandler().
  static std::string* log_string_ptr = nullptr;
  std::string log_string;
  log_string_ptr = &log_string;
  SetLogMessageHandler([](int severity, const char* file, int line,
                          size_t start, const std::string& str) -> bool {
    *log_string_ptr = str;
    return true;
  });

  log_once_unittest();
  EXPECT_NE(std::string::npos, log_string.find("This is log_once_unittest()."));

  log_once_unittest_1();
  EXPECT_NE(std::string::npos,
            log_string.find("This is log_once_unittest_1()."));

  log_once_unittest_2();
  EXPECT_NE(std::string::npos,
            log_string.find("This is log_once_unittest_2()."));

  log_string.clear();

  log_once_unittest();
  EXPECT_TRUE(log_string.empty());

  log_once_unittest_1();
  EXPECT_TRUE(log_string.empty());

  log_once_unittest_2();
  EXPECT_TRUE(log_string.empty());

  // Clean up.
  SetLogMessageHandler(old_log_message_handler);
  log_string_ptr = nullptr;
}

TEST(LogOnceTest, NotImplementedLogOnce) {
  // Set up a callback function to capture the log output string.
  auto old_log_message_handler = GetLogMessageHandler();
  // Use a static because only captureless lambdas can be converted to a
  // function pointer for SetLogMessageHandler().
  static std::string* log_string_ptr = nullptr;
  std::string log_string;
  log_string_ptr = &log_string;
  SetLogMessageHandler([](int severity, const char* file, int line,
                          size_t start, const std::string& str) -> bool {
    *log_string_ptr = str;
    return true;
  });

  notimplemented_log_once_unittest();
  EXPECT_NE(std::string::npos,
            log_string.find("This is notimplemented_log_once_unittest()."));

  notimplemented_log_once_unittest_1();
  EXPECT_NE(std::string::npos,
            log_string.find("This is notimplemented_log_once_unittest_1()."));

  notimplemented_log_once_unittest_2();
  EXPECT_NE(std::string::npos,
            log_string.find("This is notimplemented_log_once_unittest_2()."));

  log_string.clear();

  notimplemented_log_once_unittest();
  EXPECT_TRUE(log_string.empty());

  notimplemented_log_once_unittest_1();
  EXPECT_TRUE(log_string.empty());

  notimplemented_log_once_unittest_2();
  EXPECT_TRUE(log_string.empty());

  // Clean up.
  SetLogMessageHandler(old_log_message_handler);
  log_string_ptr = nullptr;
}

}  // namespace
}  // namespace logging

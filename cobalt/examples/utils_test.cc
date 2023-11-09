// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/examples/utils.h"

#include <stdarg.h>

#include "gmock/gmock.h"
#include "starboard/proxy/starboard_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockSbFileModule {
 public:
  MOCK_METHOD(bool, FileDelete, (const char*));
};

MockSbFileModule* GetMockSbFileModule() {
  static auto* const mock_sb_file_module = []() {
    auto* inner_mock_sb_file_module = new MockSbFileModule;
    // There doesn't seem to be a good way to avoid leaking the mock object.
    testing::Mock::AllowLeak(inner_mock_sb_file_module);
    return inner_mock_sb_file_module;
  }();
  return mock_sb_file_module;
}

class MockSbLogModule {
 public:
  MOCK_METHOD(void, LogFormat, (const char*, va_list));
};

MockSbLogModule* GetMockSbLogModule() {
  static auto* const mock_sb_log_module = []() {
    auto* inner_mock_sb_log_module = new MockSbLogModule;
    // There doesn't seem to be a good way to avoid leaking the mock object.
    testing::Mock::AllowLeak(inner_mock_sb_log_module);
    return inner_mock_sb_log_module;
  }();
  return mock_sb_log_module;
}

bool StubFileDeleteFn(const char* path) {
  return GetMockSbFileModule()->FileDelete(path);
}

void StubLogFormatFn(const char* path, va_list arguments) {
  return GetMockSbLogModule()->LogFormat(path, arguments);
}

class UtilsTest : public testing::Test {};

TEST_F(UtilsTest, StateChangingFunctionCallsFileDelete) {
  // Arrange
  const char* path = "path/to/some/file";
  EXPECT_CALL(*GetMockSbFileModule(), FileDelete(path))
      .Times(1)
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*GetMockSbLogModule(),
              LogFormat("Going to delete: %s", testing::_))
      .Times(1)
      .WillOnce(testing::Return());

  // Act
  ::starboard::proxy::SbProxy* sb_proxy = ::starboard::proxy::GetSbProxy();
  sb_proxy->SetFileDelete(&StubFileDeleteFn);
  // The SbLogFormat test double is registered so the test can verify that the
  // code under test does the expected logging.
  sb_proxy->SetLogFormat(&StubLogFormatFn);

  StateChangingFunction(path);

  // The proxy needs to be reset to use the platform's real SbLogFormat
  // implementation, as googletest uses SbLogFormat to output test results.
  sb_proxy->SetLogFormat(NULL);
  sb_proxy->SetFileDelete(NULL);

  // Assert
  // Since the mock object is leaked, the automatic verification on destruction
  // doesn't happen. So, we force verification here.
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(GetMockSbFileModule()));
  EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(GetMockSbLogModule()));
}

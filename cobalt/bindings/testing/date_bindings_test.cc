// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "base/logging.h"

#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/interface_with_date.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace bindings {
namespace testing {
namespace {

class DateBindingsTest : public InterfaceBindingsTest<InterfaceWithDate> {};

TEST_F(DateBindingsTest, NiceDates) {
  std::string result;

  // Date is initialized with default ctor, giving "Invalid Date".
  EXPECT_TRUE(EvaluateScript("test.getDate()", &result)) << result;
  EXPECT_STREQ("Invalid Date", result.c_str());

  // Set date to Feb. 1, 2014 (month is 0-based)
  EXPECT_TRUE(EvaluateScript("test.setDate(new Date(2014, 1, 1))", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getDate()", &result)) << result;
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "Sat Feb 01 2014 00:00:00",
                      result.c_str());

  // Set date to July 4, 1776 03:02:01 (month is 0-based)
  EXPECT_TRUE(
      EvaluateScript("test.setDate(new Date(1776, 6, 4, 3, 2, 1))", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getDate()", &result)) << result;
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "Thu Jul 04 1776 03:02:01",
                      result.c_str());

  // Set date to POSIX EPOC +1 second (month is 0-based)
  EXPECT_TRUE(
      EvaluateScript("test.setDate(new Date(1970, 0, 1, 0, 0, 1))", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getDate()", &result)) << result;
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "Thu Jan 01 1970 00:00:01",
                      result.c_str());

  // Set date to WIN32 EPOC +1 second (month is 0-based)
  EXPECT_TRUE(
      EvaluateScript("test.setDate(new Date(1601, 0, 1, 0, 0, 1))", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getDate()", &result)) << result;
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, "Mon Jan 01 1601 00:00:01",
                      result.c_str());

  // Set date back to invalid.
  EXPECT_TRUE(EvaluateScript("test.setDate(new Date(NaN))", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getDate()", &result)) << result;
  EXPECT_STREQ("Invalid Date", result.c_str());
}

}  // namespace
}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

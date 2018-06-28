// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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
#include "cobalt/bindings/testing/interface_with_any.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainsRegex;

namespace cobalt {
namespace bindings {
namespace testing {
namespace {

class AnyBindingsTest : public InterfaceBindingsTest<InterfaceWithAny> {};

TEST_F(AnyBindingsTest, All) {
  std::string result;

  // We should be able to clear it out this way.
  EXPECT_TRUE(EvaluateScript("test.setAny(undefined)", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getAny()", &result)) << result;
  EXPECT_STREQ("undefined", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.setAny(null)", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getAny()", &result)) << result;
  EXPECT_STREQ("null", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.setAny(2001)", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getAny()", &result)) << result;
  EXPECT_STREQ("2001", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.setAny(1.21)", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getAny()", &result)) << result;
  EXPECT_STREQ("1.21", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.setAny('test')", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getAny()", &result)) << result;
  EXPECT_STREQ("test", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.setAny(new Object())", &result)) << result;
  EXPECT_TRUE(EvaluateScript("test.getAny()", &result)) << result;
  EXPECT_STREQ("[object Object]", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.setAny(new ArbitraryInterface())", &result))
      << result;
  EXPECT_TRUE(EvaluateScript("test.getAny()", &result)) << result;
  EXPECT_STREQ("[object ArbitraryInterface]", result.c_str());
}

}  // namespace
}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

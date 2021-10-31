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

#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/window_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef BindingsTestBase ConstantsBindingsTest;
}  // namespace

TEST_F(ConstantsBindingsTest, DefinedOnConstructor) {
  std::string result;
  EXPECT_TRUE(
      EvaluateScript("ConstantsInterface.INTEGER_CONSTANT == 5;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("ConstantsInterface.DOUBLE_CONSTANT == 2.718;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(ConstantsBindingsTest, DefinedOnPrototype) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "ConstantsInterface.prototype.INTEGER_CONSTANT == 5;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "ConstantsInterface.prototype.DOUBLE_CONSTANT == 2.718;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(ConstantsBindingsTest, ReadOnly) {
  std::string result;
  EXPECT_TRUE(
      EvaluateScript("ConstantsInterface.INTEGER_CONSTANT = 1000;", &result));
  EXPECT_TRUE(
      EvaluateScript("ConstantsInterface.INTEGER_CONSTANT == 5;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("ConstantsInterface.DOUBLE_CONSTANT = 3.14;", &result));
  EXPECT_TRUE(
      EvaluateScript("ConstantsInterface.DOUBLE_CONSTANT == 2.718;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "ConstantsInterface.prototype.INTEGER_CONSTANT = 1000;", &result));
  EXPECT_TRUE(EvaluateScript(
      "ConstantsInterface.prototype.INTEGER_CONSTANT == 5;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "ConstantsInterface.prototype.DOUBLE_CONSTANT = 3.14;", &result));
  EXPECT_TRUE(EvaluateScript(
      "ConstantsInterface.prototype.DOUBLE_CONSTANT == 2.718;", &result));
  EXPECT_STREQ("true", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

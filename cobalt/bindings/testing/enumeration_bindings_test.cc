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
#include "cobalt/bindings/testing/enumeration_interface.h"
#include "cobalt/bindings/testing/test_enum.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<EnumerationInterface> EnumerationBindingsTest;
}  // namespace

TEST_F(EnumerationBindingsTest, SetEnumeration) {
  InSequence dummy;

  EXPECT_CALL(test_mock(), set_enum_property(kTestEnumAlpha));
  EXPECT_TRUE(EvaluateScript("test.enumProperty = \"alpha\";", NULL));

  EXPECT_CALL(test_mock(), set_enum_property(kTestEnumBeta));
  EXPECT_TRUE(EvaluateScript("test.enumProperty = \"beta\";", NULL));

  EXPECT_CALL(test_mock(), set_enum_property(kTestEnumGamma));
  EXPECT_TRUE(EvaluateScript("test.enumProperty = \"gamma\";", NULL));
}

TEST_F(EnumerationBindingsTest, GetEnumeration) {
  InSequence dummy;
  std::string result;

  EXPECT_CALL(test_mock(), enum_property()).WillOnce(Return(kTestEnumAlpha));
  EXPECT_TRUE(EvaluateScript("test.enumProperty == \"alpha\";", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), enum_property()).WillOnce(Return(kTestEnumBeta));
  EXPECT_TRUE(EvaluateScript("test.enumProperty == \"beta\";", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), enum_property()).WillOnce(Return(kTestEnumGamma));
  EXPECT_TRUE(EvaluateScript("test.enumProperty == \"gamma\";", &result));
  EXPECT_STREQ("true", result.c_str());
}

// http://heycam.github.io/webidl/#es-enumeration
// If S is not one of E's enumeration values, then throw a TypeError.
TEST_F(EnumerationBindingsTest, SetInvalidValue) {
  InSequence dummy;

  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "var caught_exception = null; "
      "try { test.enumProperty = \"invalid\"; } "
      "catch (e) { caught_exception = e }", NULL));
  EXPECT_TRUE(
      EvaluateScript("caught_exception.constructor === TypeError;", &result));
  EXPECT_STREQ("true", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

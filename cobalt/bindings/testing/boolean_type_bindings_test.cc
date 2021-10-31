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
#include "cobalt/bindings/testing/boolean_type_test_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<BooleanTypeTestInterface> BooleanTypeBindingsTest;
}  // namespace

// Boolean conversion algorithm: https://www.w3.org/TR/WebIDL/#es-boolean
// ToBoolean operation: http://es5.github.io/#x9.2

TEST_F(BooleanTypeBindingsTest, GetBooleanProperty) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(), boolean_property()).WillOnce(Return(true));
  EXPECT_TRUE(EvaluateScript("var result = test.booleanProperty;", NULL));

  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof result;", &result));
  EXPECT_STREQ("boolean", result.c_str());

  EXPECT_TRUE(EvaluateScript("result;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), boolean_property()).WillOnce(Return(false));
  EXPECT_TRUE(EvaluateScript("var result = test.booleanProperty;", NULL));

  EXPECT_TRUE(EvaluateScript("typeof result;", &result));
  EXPECT_STREQ("boolean", result.c_str());

  EXPECT_TRUE(EvaluateScript("result;", &result));
  EXPECT_STREQ("false", result.c_str());
}

TEST_F(BooleanTypeBindingsTest, BooleanReturnValue) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(), BooleanReturnOperation()).WillOnce(Return(true));
  EXPECT_TRUE(
      EvaluateScript("var result = test.booleanReturnOperation();", NULL));

  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof result;", &result));
  EXPECT_STREQ("boolean", result.c_str());

  EXPECT_TRUE(EvaluateScript("result;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), BooleanReturnOperation()).WillOnce(Return(false));
  EXPECT_TRUE(
      EvaluateScript("var result = test.booleanReturnOperation();", NULL));

  EXPECT_TRUE(EvaluateScript("typeof result;", &result));
  EXPECT_STREQ("boolean", result.c_str());

  EXPECT_TRUE(EvaluateScript("result;", &result));
  EXPECT_STREQ("false", result.c_str());
}

TEST_F(BooleanTypeBindingsTest, SetBooleanProperty) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(), set_boolean_property(true));
  EXPECT_TRUE(EvaluateScript("test.booleanProperty = true;", NULL));

  EXPECT_CALL(test_mock(), set_boolean_property(false));
  EXPECT_TRUE(EvaluateScript("test.booleanProperty = false;", NULL));

  EXPECT_CALL(test_mock(), set_boolean_property(false));
  EXPECT_TRUE(EvaluateScript("test.booleanProperty = undefined;", NULL));

  EXPECT_CALL(test_mock(), set_boolean_property(false));
  EXPECT_TRUE(EvaluateScript("test.booleanProperty = null;", NULL));

  EXPECT_CALL(test_mock(), set_boolean_property(false));
  EXPECT_TRUE(EvaluateScript("test.booleanProperty = 0;", NULL));

  EXPECT_CALL(test_mock(), set_boolean_property(false));
  EXPECT_TRUE(EvaluateScript("test.booleanProperty = NaN;", NULL));

  EXPECT_CALL(test_mock(), set_boolean_property(true));
  EXPECT_TRUE(EvaluateScript("test.booleanProperty = 1;", NULL));

  EXPECT_CALL(test_mock(), set_boolean_property(true));
  EXPECT_TRUE(EvaluateScript("test.booleanProperty = -1;", NULL));

  EXPECT_CALL(test_mock(), set_boolean_property(true));
  EXPECT_TRUE(EvaluateScript("test.booleanProperty = \"string\";", NULL));

  EXPECT_CALL(test_mock(), set_boolean_property(false));
  EXPECT_TRUE(EvaluateScript("test.booleanProperty = \"\";", NULL));

  // An arbitrary non-null object should convert to 'true'
  EXPECT_CALL(test_mock(), set_boolean_property(true));
  EXPECT_TRUE(EvaluateScript("test.booleanProperty = test;", NULL));
}

TEST_F(BooleanTypeBindingsTest, BooleanArgumentOperation) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(), BooleanArgumentOperation(true));
  EXPECT_TRUE(EvaluateScript("test.booleanArgumentOperation(true);", NULL));

  EXPECT_CALL(test_mock(), BooleanArgumentOperation(false));
  EXPECT_TRUE(EvaluateScript("test.booleanArgumentOperation(false);", NULL));

  EXPECT_CALL(test_mock(), BooleanArgumentOperation(false));
  EXPECT_TRUE(
      EvaluateScript("test.booleanArgumentOperation(undefined);", NULL));

  EXPECT_CALL(test_mock(), BooleanArgumentOperation(false));
  EXPECT_TRUE(EvaluateScript("test.booleanArgumentOperation(null);", NULL));

  EXPECT_CALL(test_mock(), BooleanArgumentOperation(false));
  EXPECT_TRUE(EvaluateScript("test.booleanArgumentOperation(0);", NULL));

  EXPECT_CALL(test_mock(), BooleanArgumentOperation(false));
  EXPECT_TRUE(EvaluateScript("test.booleanArgumentOperation(NaN);", NULL));

  EXPECT_CALL(test_mock(), BooleanArgumentOperation(true));
  EXPECT_TRUE(EvaluateScript("test.booleanArgumentOperation(1);", NULL));

  EXPECT_CALL(test_mock(), BooleanArgumentOperation(true));
  EXPECT_TRUE(EvaluateScript("test.booleanArgumentOperation(-1);", NULL));

  EXPECT_CALL(test_mock(), BooleanArgumentOperation(true));
  EXPECT_TRUE(
      EvaluateScript("test.booleanArgumentOperation(\"string\");", NULL));

  EXPECT_CALL(test_mock(), set_boolean_property(false));
  EXPECT_TRUE(EvaluateScript("test.booleanProperty = \"\";", NULL));

  // An arbitrary non-null object should convert to 'true'
  EXPECT_CALL(test_mock(), BooleanArgumentOperation(true));
  EXPECT_TRUE(EvaluateScript("test.booleanArgumentOperation(test);", NULL));
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

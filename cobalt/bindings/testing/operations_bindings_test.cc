/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/operations_test_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<OperationsTestInterface> OperationsBindingsTest;
}  // namespace

TEST_F(OperationsBindingsTest, VoidFunction) {
  EXPECT_CALL(test_mock(), VoidFunctionNoArgs());

  EXPECT_TRUE(EvaluateScript("test.voidFunctionNoArgs();", NULL));
}

TEST_F(OperationsBindingsTest, StringFunction) {
  EXPECT_CALL(test_mock(), StringFunctionNoArgs())
      .WillOnce(Return("mock_value"));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.stringFunctionNoArgs();", &result));
  EXPECT_STREQ("mock_value", result.c_str());
}

TEST_F(OperationsBindingsTest, ObjectFunction) {
  scoped_refptr<ArbitraryInterface> object = new ArbitraryInterface();

  EXPECT_CALL(test_mock(), ObjectFunctionNoArgs()).WillOnce(Return(object));
  EXPECT_TRUE(
      EvaluateScript("var object = test.objectFunctionNoArgs();", NULL));

  EXPECT_CALL(*object, ArbitraryFunction());
  EXPECT_TRUE(EvaluateScript("object.arbitraryFunction();", NULL));
}

TEST_F(OperationsBindingsTest, VoidFunctionStringArg) {
  EXPECT_CALL(test_mock(), VoidFunctionStringArg("mock_value"));
  EXPECT_TRUE(
      EvaluateScript("test.voidFunctionStringArg(\"mock_value\");", NULL));
}

TEST_F(OperationsBindingsTest, VoidFunctionObjectArg) {
  scoped_refptr<ArbitraryInterface> object = new ArbitraryInterface();

  EXPECT_CALL(test_mock(), ObjectFunctionNoArgs()).WillOnce(Return(object));
  EXPECT_TRUE(
      EvaluateScript("var object = test.objectFunctionNoArgs();", NULL));

  EXPECT_CALL(test_mock(), VoidFunctionObjectArg(object));
  EXPECT_TRUE(EvaluateScript("test.voidFunctionObjectArg(object);", NULL));
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

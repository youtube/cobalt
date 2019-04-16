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

#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/operations_test_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::A;
using ::testing::InSequence;
using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<OperationsTestInterface> OperationsBindingsTest;
}  // namespace

TEST_F(OperationsBindingsTest, OperationIsPrototypeProperty) {
  std::string result;
  EXPECT_TRUE(
      EvaluateScript("test.hasOwnProperty(\"voidFunctionNoArgs\");", &result));
  EXPECT_STREQ("false", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(test).hasOwnProperty(\"voidFunctionNoArgs\");",
      &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(OperationsBindingsTest, LengthPropertyIsSet) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("test.voidFunctionNoArgs.length;", &result));
  EXPECT_STREQ("0", result.c_str());

  EXPECT_TRUE(EvaluateScript("test.voidFunctionStringArg.length;", &result));
  EXPECT_STREQ("1", result.c_str());
}

TEST_F(OperationsBindingsTest, VoidFunction) {
  EXPECT_CALL(test_mock(), VoidFunctionNoArgs());

  EXPECT_TRUE(EvaluateScript("test.voidFunctionNoArgs();", NULL));
}

TEST_F(OperationsBindingsTest, LongFunction) {
  EXPECT_CALL(test_mock(), LongFunctionNoArgs()).WillOnce(Return(5));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.longFunctionNoArgs();", &result));
  EXPECT_STREQ("5", result.c_str());
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

TEST_F(OperationsBindingsTest, VoidFunctionLongArg) {
  EXPECT_CALL(test_mock(), VoidFunctionLongArg(42));
  EXPECT_TRUE(EvaluateScript("test.voidFunctionLongArg(42);", NULL));
}

TEST_F(OperationsBindingsTest, VoidFunctionObjectArg) {
  scoped_refptr<ArbitraryInterface> object = new ArbitraryInterface();

  EXPECT_CALL(test_mock(), ObjectFunctionNoArgs()).WillOnce(Return(object));
  EXPECT_TRUE(
      EvaluateScript("var object = test.objectFunctionNoArgs();", NULL));

  EXPECT_CALL(test_mock(), VoidFunctionObjectArg(object));
  EXPECT_TRUE(EvaluateScript("test.voidFunctionObjectArg(object);", NULL));
}

TEST_F(OperationsBindingsTest, OptionalArguments) {
  EXPECT_CALL(test_mock(), OptionalArguments(4));
  EXPECT_TRUE(EvaluateScript("test.optionalArguments(4);", NULL));

  EXPECT_CALL(test_mock(), OptionalArguments(4, 1));
  EXPECT_TRUE(EvaluateScript("test.optionalArguments(4, 1);", NULL));

  EXPECT_CALL(test_mock(), OptionalArguments(4, 1, -6));
  EXPECT_TRUE(EvaluateScript("test.optionalArguments(4, 1, -6);", NULL));

  EXPECT_CALL(test_mock(), OptionalArgumentWithDefault(2.718));
  EXPECT_TRUE(EvaluateScript("test.optionalArgumentWithDefault();", NULL));

  EXPECT_CALL(test_mock(), OptionalArgumentWithDefault(3.14));
  EXPECT_TRUE(EvaluateScript("test.optionalArgumentWithDefault(3.14);", NULL));
}

TEST_F(OperationsBindingsTest, OverloadedOperationByNumberOfArguments) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(), OverloadedFunction());
  EXPECT_TRUE(EvaluateScript("test.overloadedFunction();", NULL));

  EXPECT_CALL(test_mock(), OverloadedFunction(_, _, A<int32_t>()));
  EXPECT_TRUE(EvaluateScript("test.overloadedFunction(6, 8, 9);", NULL));

  EXPECT_CALL(
      test_mock(),
      OverloadedFunction(_, _, A<const scoped_refptr<ArbitraryInterface>&>()));
  EXPECT_TRUE(EvaluateScript(
      "test.overloadedFunction(6, 8, new ArbitraryInterface());", NULL));
}

TEST_F(OperationsBindingsTest, OverloadedOperationInvalidNumberOfArguments) {
  InSequence in_sequence_dummy;

  EXPECT_TRUE(EvaluateScript(
      "var error = null;"
      "try { test.overloadedFunction(1, 2); }"
      "catch(e) { error = e; }",
      NULL));
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(error) === TypeError.prototype;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(OperationsBindingsTest, OverloadedOperationByType) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(), OverloadedFunction(A<int32_t>()));
  EXPECT_TRUE(EvaluateScript("test.overloadedFunction(4);", NULL));

  EXPECT_CALL(test_mock(), OverloadedFunction(A<const std::string&>()));
  EXPECT_TRUE(EvaluateScript("test.overloadedFunction(\"foo\");", NULL));
}

TEST_F(OperationsBindingsTest, StaticMethodNotPartOfOverloadSet) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(OperationsTestInterface::static_methods_mock.Get(),
              OverloadedFunction(_));
  EXPECT_TRUE(
      EvaluateScript("OperationsTestInterface.overloadedFunction(6.1);", NULL));

  EXPECT_CALL(OperationsTestInterface::static_methods_mock.Get(),
              OverloadedFunction(_, _));
  EXPECT_TRUE(EvaluateScript(
      "OperationsTestInterface.overloadedFunction(4, 8);", NULL));
}

TEST_F(OperationsBindingsTest, OverloadedOperationByOptionality) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(), OverloadedNullable(A<int32_t>()));
  EXPECT_TRUE(EvaluateScript("test.overloadedNullable(5);", NULL));

  EXPECT_CALL(test_mock(), OverloadedNullable(base::Optional<bool>()));
  EXPECT_TRUE(EvaluateScript("test.overloadedNullable(null);", NULL));
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

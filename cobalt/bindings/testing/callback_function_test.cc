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

#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/callback_function_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<CallbackFunctionInterface> CallbackFunctionTest;
}  // namespace

TEST_F(CallbackFunctionTest, ScriptCallbackCanBeCalledFromC) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("var was_called = false;", NULL));

  scoped_refptr<CallbackFunctionInterface::VoidFunction> void_function;
  EXPECT_CALL(test_mock(), TakesVoidFunction(_))
      .WillOnce(SaveArg<0>(&void_function));
  EXPECT_TRUE(EvaluateScript(
      "test.takesVoidFunction(function() { was_called = true });", NULL));
  ASSERT_NE(NULL, reinterpret_cast<intptr_t>(void_function.get()));

  void_function->Run();
  EXPECT_TRUE(EvaluateScript("was_called;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(CallbackFunctionTest, ScriptFunctionIsNotGarbageCollected) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("var num_called = 0;", NULL));

  scoped_refptr<CallbackFunctionInterface::VoidFunction> void_function;
  EXPECT_CALL(test_mock(), TakesVoidFunction(_))
      .WillOnce(SaveArg<0>(&void_function));
  EXPECT_TRUE(EvaluateScript(
      "test.takesVoidFunction(function() { num_called++ });", NULL));
  ASSERT_NE(NULL, reinterpret_cast<intptr_t>(void_function.get()));

  void_function->Run();
  EXPECT_TRUE(EvaluateScript("num_called == 1;", &result));
  EXPECT_STREQ("true", result.c_str());

  engine_->CollectGarbage();

  // Call it once more to ensure the function has not been garbage collected.
  void_function->Run();
  EXPECT_TRUE(EvaluateScript("num_called == 2;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(CallbackFunctionTest, CallbackWithOneParameter) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("var callback_value;", NULL));

  // Store a handle to the callback passed from script.
  scoped_refptr<CallbackFunctionInterface::FunctionWithOneParameter>
      function_with_parameter;
  EXPECT_CALL(test_mock(), TakesFunctionWithOneParameter(_))
      .WillOnce(SaveArg<0>(&function_with_parameter));
  EXPECT_TRUE(EvaluateScript(
      "test.takesFunctionWithOneParameter(function(value) { "
      "callback_value = value });",
      NULL));
  ASSERT_NE(NULL, reinterpret_cast<intptr_t>(function_with_parameter.get()));

  // Run the callback, and check that the value was passed through to script.
  function_with_parameter->Run(5);
  EXPECT_TRUE(EvaluateScript("callback_value;", &result));
  EXPECT_STREQ("5", result.c_str());
}

TEST_F(CallbackFunctionTest, CallbackWithSeveralParameters) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("var value1;", NULL));
  EXPECT_TRUE(EvaluateScript("var value2;", NULL));
  EXPECT_TRUE(EvaluateScript("var value3;", NULL));

  // Store a handle to the callback passed from script.
  scoped_refptr<CallbackFunctionInterface::FunctionWithSeveralParameters>
      function_with_several_parameters;
  EXPECT_CALL(test_mock(), TakesFunctionWithSeveralParameters(_))
      .WillOnce(SaveArg<0>(&function_with_several_parameters));
  EXPECT_TRUE(EvaluateScript(
      "test.takesFunctionWithSeveralParameters("
      "function(param1, param2, param3) { "
      "value1 = param1; value2 = param2; value3 = param3; });",
      NULL));
  ASSERT_NE(NULL,
            reinterpret_cast<intptr_t>(function_with_several_parameters.get()));

  // Execute the callback
  scoped_refptr<ArbitraryInterface> some_object =
      new ::testing::StrictMock<ArbitraryInterface>();
  function_with_several_parameters->Run(3.14, "some string", some_object);

  // Verify that each parameter got passed to script as expected.
  EXPECT_TRUE(EvaluateScript("value1;", &result));
  EXPECT_STREQ("3.14", result.c_str());

  EXPECT_TRUE(EvaluateScript("value2;", &result));
  EXPECT_STREQ("some string", result.c_str());

  EXPECT_CALL((*some_object.get()), ArbitraryFunction());
  EXPECT_TRUE(EvaluateScript("value3.arbitraryFunction();", NULL));
}

TEST_F(CallbackFunctionTest, CallbackWithNullableParameters) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("var value1;", NULL));
  EXPECT_TRUE(EvaluateScript("var value2;", NULL));
  EXPECT_TRUE(EvaluateScript("var value3;", NULL));

  // Store a handle to the callback passed from script.
  scoped_refptr<CallbackFunctionInterface::FunctionWithNullableParameters>
      function_with_nullable_parameters;
  EXPECT_CALL(test_mock(), TakesFunctionWithNullableParameters(_))
      .WillOnce(SaveArg<0>(&function_with_nullable_parameters));
  EXPECT_TRUE(EvaluateScript(
      "test.takesFunctionWithNullableParameters("
      "function(param1, param2, param3) { "
      "value1 = param1; value2 = param2; value3 = param3; });",
      NULL));
  ASSERT_NE(NULL, reinterpret_cast<intptr_t>(
                      function_with_nullable_parameters.get()));

  // Execute the callback
  function_with_nullable_parameters->Run(base::nullopt, base::nullopt, NULL);

  // Verify that each parameter got passed to script as expected.
  EXPECT_TRUE(EvaluateScript("value1;", &result));
  EXPECT_STREQ("null", result.c_str());

  EXPECT_TRUE(EvaluateScript("value2;", &result));
  EXPECT_STREQ("null", result.c_str());

  EXPECT_TRUE(EvaluateScript("value3;", &result));
  EXPECT_STREQ("null", result.c_str());
}

TEST_F(CallbackFunctionTest, CallbackAttribute) {
  InSequence in_sequence_dummy;

  std::string result;
  EXPECT_TRUE(EvaluateScript("var num_called = 0;", NULL));

  scoped_refptr<CallbackFunctionInterface::VoidFunction> void_function;
  EXPECT_CALL(test_mock(), set_callback_attribute(_))
      .WillOnce(SaveArg<0>(&void_function));
  EXPECT_TRUE(EvaluateScript(
      "var callback_function = function() { ++num_called; };", NULL));
  EXPECT_TRUE(
      EvaluateScript("test.callbackAttribute = callback_function;", NULL));
  ASSERT_NE(NULL, reinterpret_cast<intptr_t>(void_function.get()));

  // Execute the callback
  void_function->Run();
  EXPECT_TRUE(EvaluateScript("num_called;", &result));
  EXPECT_STREQ("1", result.c_str());

  // Check that the getter references the same object
  EXPECT_CALL(test_mock(), callback_attribute())
      .WillOnce(Return(void_function));
  EXPECT_TRUE(
      EvaluateScript("test.callbackAttribute === callback_function;", &result));
  EXPECT_STREQ("true", result.c_str());

  // Get the callback and execute it
  EXPECT_CALL(test_mock(), callback_attribute())
      .WillOnce(Return(void_function));
  EXPECT_TRUE(
      EvaluateScript("var callback_function2 = test.callbackAttribute;", NULL));
  EXPECT_TRUE(EvaluateScript("callback_function2();", NULL));
  EXPECT_TRUE(EvaluateScript("num_called;", &result));
  EXPECT_STREQ("2", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

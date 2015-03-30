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

  CallbackFunctionInterface::VoidFunction void_function;
  EXPECT_CALL(test_mock(), TakesVoidFunction(_))
      .WillOnce(SaveArg<0>(&void_function));
  EXPECT_TRUE(EvaluateScript(
      "test.takesVoidFunction(function() { was_called = true });", NULL));
  ASSERT_FALSE(void_function.is_null());

  void_function.Run();
  EXPECT_TRUE(EvaluateScript("was_called;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(CallbackFunctionTest, ScriptFunctionIsNotGarbageCollected) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("var num_called = 0;", NULL));

  CallbackFunctionInterface::VoidFunction void_function;
  EXPECT_CALL(test_mock(), TakesVoidFunction(_))
      .WillOnce(SaveArg<0>(&void_function));
  EXPECT_TRUE(EvaluateScript(
      "test.takesVoidFunction(function() { num_called++ });", NULL));
  ASSERT_FALSE(void_function.is_null());

  void_function.Run();
  EXPECT_TRUE(EvaluateScript("num_called == 1;", &result));
  EXPECT_STREQ("true", result.c_str());

  engine_->CollectGarbage();

  // Call it once more to ensure the function has not been garbage collected.
  void_function.Run();
  EXPECT_TRUE(EvaluateScript("num_called == 2;", &result));
  EXPECT_STREQ("true", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

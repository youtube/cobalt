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

#include "base/strings/stringprintf.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/exception_object_interface.h"
#include "cobalt/bindings/testing/exceptions_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::NiceMock;
using ::testing::WithArg;

#define EXPECT_SUBSTRING(needle, haystack) \
  EXPECT_PRED_FORMAT2(::testing::IsSubstring, (needle), (haystack))

namespace cobalt {
namespace bindings {
namespace testing {
namespace {
typedef InterfaceBindingsTest<ExceptionsInterface> ExceptionsBindingsTest;

class SimpleExceptionSetter {
 public:
  explicit SimpleExceptionSetter(script::MessageType type) : type_(type) {}
  void FireException(script::ExceptionState* exception) {
    exception->SetSimpleException(type_);
  }

 private:
  script::MessageType type_;
};

class ExceptionObjectSetter {
 public:
  explicit ExceptionObjectSetter(
      const scoped_refptr<ExceptionObjectInterface>& exception)
      : exception_object_(exception) {}
  void FireException(script::ExceptionState* exception) {
    exception->SetException(exception_object_);
  }

 private:
  scoped_refptr<script::ScriptException> exception_object_;
};

}  // namespace

TEST_F(ExceptionsBindingsTest, ThrowExceptionFromConstructor) {
  SimpleExceptionSetter exception_setter(script::kSimpleError);
  EXPECT_CALL(ExceptionsInterface::constructor_implementation_mock.Get(),
              Constructor(_))
      .WillOnce(
          Invoke(&exception_setter, &SimpleExceptionSetter::FireException));

  std::string result;
  EXPECT_FALSE(EvaluateScript("var foo = new ExceptionsInterface();", &result));
  EXPECT_SUBSTRING("Error", result.c_str());
}

TEST_F(ExceptionsBindingsTest, ThrowExceptionFromOperation) {
  SimpleExceptionSetter exception_setter(script::kSimpleTypeError);
  EXPECT_CALL(test_mock(), FunctionThrowsException(_)).WillOnce(
      Invoke(&exception_setter, &SimpleExceptionSetter::FireException));

  std::string result;
  EXPECT_FALSE(EvaluateScript("test.functionThrowsException();", &result));
  EXPECT_SUBSTRING("TypeError", result.c_str());
}

TEST_F(ExceptionsBindingsTest, ThrowExceptionFromGetter) {
  SimpleExceptionSetter exception_setter(script::kSimpleRangeError);
  EXPECT_CALL(test_mock(), attribute_throws_exception(_)).WillOnce(
      DoAll(Invoke(&exception_setter, &SimpleExceptionSetter::FireException),
            Return(false)));

  std::string result;
  EXPECT_FALSE(
      EvaluateScript("var foo = test.attributeThrowsException;", &result));
  EXPECT_SUBSTRING("RangeError", result.c_str());
}

TEST_F(ExceptionsBindingsTest, ThrowExceptionFromSetter) {
  SimpleExceptionSetter exception_setter(script::kSimpleReferenceError);
  EXPECT_CALL(test_mock(), set_attribute_throws_exception(_, _))
      .WillOnce(WithArg<1>(
          Invoke(&exception_setter, &SimpleExceptionSetter::FireException)));

  std::string result;
  EXPECT_FALSE(
      EvaluateScript("test.attributeThrowsException = true;", &result));
  EXPECT_SUBSTRING("ReferenceError", result.c_str());
}

TEST_F(ExceptionsBindingsTest, ThrowExceptionObject) {
  scoped_refptr<ExceptionObjectInterface> exception_object =
      base::WrapRefCounted(new NiceMock<ExceptionObjectInterface>());
  ExceptionObjectSetter exception_setter(exception_object);

  InSequence in_sequence_dummy;
  EXPECT_CALL(test_mock(), FunctionThrowsException(_)).WillOnce(
      Invoke(&exception_setter, &ExceptionObjectSetter::FireException));
  EXPECT_TRUE(EvaluateScript(
      "var error = null;"
      "try { test.functionThrowsException(); }"
      "catch(e) { error = e; }",
      NULL));

  std::string result;
  EXPECT_TRUE(EvaluateScript("error instanceof Error;", &result));
  EXPECT_STREQ("true", result.c_str());
  EXPECT_TRUE(
      EvaluateScript("error instanceof ExceptionObjectInterface;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(*exception_object, error()).WillOnce(Return("the error"));
  EXPECT_TRUE(EvaluateScript("error.error;", &result));
  EXPECT_STREQ("the error", result.c_str());

  EXPECT_CALL(*exception_object, message()).WillOnce(Return("the message"));
  EXPECT_TRUE(EvaluateScript("error.message;", &result));
  EXPECT_STREQ("the message", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

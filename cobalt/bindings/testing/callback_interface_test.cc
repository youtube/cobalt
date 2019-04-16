// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "base/optional.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/callback_interface_interface.h"
#include "cobalt/bindings/testing/script_object_owner.h"
#include "cobalt/bindings/testing/single_operation_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainsRegex;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
class CallbackInterfaceTest
    : public InterfaceBindingsTest<CallbackInterfaceInterface> {
 public:
  typedef ScriptObjectOwner<script::ScriptValue<SingleOperationInterface> >
      InterfaceOwner;
  CallbackInterfaceTest()
      : callback_arg_(new ArbitraryInterface()), had_exception_(true) {}

 protected:
  // The argument that will be passed to the CallbackInterface's function.
  scoped_refptr<ArbitraryInterface> callback_arg_;
  bool had_exception_;
};
}  // namespace

TEST_F(CallbackInterfaceTest, ObjectCanImplementInterface) {
  std::string result;

  InterfaceOwner interface_owner(&test_mock());
  EXPECT_CALL(test_mock(), RegisterCallback(_))
      .WillOnce(Invoke(&interface_owner, &InterfaceOwner::TakeOwnership));

  // Register an object that implements the callback interface.
  EXPECT_TRUE(
      EvaluateScript("var someFunction = function(arg) { "
                     "  this.someOperation();"
                     "  arg.arbitraryFunction();"
                     "  return 7;"
                     "};"
                     "var implementer = {};"
                     "test.registerCallback(implementer);"));
  ASSERT_TRUE(interface_owner.IsSet());

  // Try executing the callback. There should be a TypeError because there is no
  // "handleCallback" property set.
  interface_owner.reference().value().HandleCallback(test_mock_, callback_arg_,
                                                     &had_exception_);
  EXPECT_TRUE(had_exception_);

  // Set the "handleCallback" property.
  EXPECT_TRUE(EvaluateScript("implementer.handleCallback = someFunction;"));

  // Execute the callback.
  EXPECT_CALL(*callback_arg_, ArbitraryFunction());
  EXPECT_CALL(test_mock(), SomeOperation());
  had_exception_ = true;
  base::Optional<int32_t> return_value =
      interface_owner.reference().value().HandleCallback(
          test_mock_, callback_arg_, &had_exception_);
  EXPECT_FALSE(had_exception_);
  EXPECT_EQ(7, return_value);
}

TEST_F(CallbackInterfaceTest, FunctionCanImplementInterface) {
  std::string result;

  InterfaceOwner interface_owner(&test_mock());
  EXPECT_CALL(test_mock(), RegisterCallback(_))
      .WillOnce(Invoke(&interface_owner, &InterfaceOwner::TakeOwnership));

  // Register a function for the callback interface.
  EXPECT_TRUE(
      EvaluateScript("var someFunction = function(arg) { "
                     "  this.someOperation();"
                     "  arg.arbitraryFunction();"
                     "  return 7;"
                     "};"
                     "test.registerCallback(someFunction);"));
  ASSERT_TRUE(interface_owner.IsSet());

  // Execute the callback.
  EXPECT_CALL(*callback_arg_, ArbitraryFunction());
  EXPECT_CALL(test_mock(), SomeOperation());
  had_exception_ = true;
  base::Optional<int32_t> return_value =
      interface_owner.reference().value().HandleCallback(
          test_mock_, callback_arg_, &had_exception_);
  EXPECT_FALSE(had_exception_);
  ASSERT_TRUE(return_value);
  EXPECT_EQ(7, *return_value);
}

TEST_F(CallbackInterfaceTest, PropertyOnObjectIsNotCallable) {
  std::string result;

  InterfaceOwner interface_owner(&test_mock());
  EXPECT_CALL(test_mock(), RegisterCallback(_))
      .WillOnce(Invoke(&interface_owner, &InterfaceOwner::TakeOwnership));

  // Define an object that has the correct property, but the property value is
  // not callable.
  EXPECT_TRUE(
      EvaluateScript("var implementer = {};"
                     "implementer.handleCallback = 45;"
                     "test.registerCallback(implementer);"));
  ASSERT_TRUE(interface_owner.IsSet());

  // Execute the callback. Callback should not have been fired.
  had_exception_ = false;
  interface_owner.reference().value().HandleCallback(test_mock_, callback_arg_,
                                                     &had_exception_);
  EXPECT_TRUE(had_exception_);
}

TEST_F(CallbackInterfaceTest, ObjectThatDoesNotImplementInterface) {
  std::string result;

  InterfaceOwner interface_owner(&test_mock());
  EXPECT_CALL(test_mock(), RegisterCallback(_))
      .WillOnce(Invoke(&interface_owner, &InterfaceOwner::TakeOwnership));

  // Set a function to the wrong property name on an object.
  EXPECT_TRUE(
      EvaluateScript("var someFunction = function(arg) { "
                     "  this.someOperation();"
                     "  arg.arbitraryFunction();"
                     "};"
                     "var implementer = {};"
                     "implementer.wrongOne = someFunction;"
                     "test.registerCallback(implementer);"));
  ASSERT_TRUE(interface_owner.IsSet());

  // Execute the callback. Callback should not have been fired.
  had_exception_ = false;
  interface_owner.reference().value().HandleCallback(test_mock_, callback_arg_,
                                                     &had_exception_);
  EXPECT_TRUE(had_exception_);
}

TEST_F(CallbackInterfaceTest, NotAnObjectOrFunction) {
  // TypeError should occur.
  std::string result;
  EXPECT_FALSE(EvaluateScript("test.registerCallback(\"foo\");", &result));
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));
}

TEST_F(CallbackInterfaceTest, ExceptionInCallback) {
  std::string result;

  InterfaceOwner interface_owner(&test_mock());
  EXPECT_CALL(test_mock(), RegisterCallback(_))
      .WillOnce(Invoke(&interface_owner, &InterfaceOwner::TakeOwnership));

  // Set up a function that will throw an exception and pass it as a callback
  // interface.
  EXPECT_TRUE(
      EvaluateScript("var someFunction = function(arg) { "
                     "  arg.arbitraryFunction();"
                     "  throw \"SomeException\";"
                     "};"
                     "test.registerCallback(someFunction);"));
  ASSERT_TRUE(interface_owner.IsSet());

  // Execute the callback. Callback should be fired but exception occurs.
  EXPECT_CALL(*callback_arg_, ArbitraryFunction());
  bool had_exception = true;
  interface_owner.reference().value().HandleCallback(test_mock_, callback_arg_,
                                                     &had_exception);
  EXPECT_TRUE(had_exception);
}

TEST_F(CallbackInterfaceTest, FunctionsAreEqual) {
  std::string result;

  InterfaceOwner interface_owner_first(&test_mock());
  InterfaceOwner interface_owner_second(&test_mock());
  EXPECT_CALL(test_mock(), RegisterCallback(_))
      .Times(2)
      .WillOnce(Invoke(&interface_owner_first, &InterfaceOwner::TakeOwnership))
      .WillOnce(
          Invoke(&interface_owner_second, &InterfaceOwner::TakeOwnership));

  EXPECT_TRUE(
      EvaluateScript("var someFunction = function(arg) { };"
                     "test.registerCallback(someFunction);"
                     "test.registerCallback(someFunction);"));
  ASSERT_TRUE(interface_owner_first.IsSet());
  ASSERT_TRUE(interface_owner_second.IsSet());

  EXPECT_TRUE(interface_owner_first.reference().referenced_value().EqualTo(
      interface_owner_second.reference().referenced_value()));
}

TEST_F(CallbackInterfaceTest, FunctionsAreNotEqual) {
  std::string result;

  InterfaceOwner interface_owner_first(&test_mock());
  InterfaceOwner interface_owner_second(&test_mock());
  EXPECT_CALL(test_mock(), RegisterCallback(_))
      .Times(2)
      .WillOnce(Invoke(&interface_owner_first, &InterfaceOwner::TakeOwnership))
      .WillOnce(
          Invoke(&interface_owner_second, &InterfaceOwner::TakeOwnership));

  EXPECT_TRUE(
      EvaluateScript("var someFunction = function(arg) { };"
                     "var someOtherFunction = function(arg) { };"
                     "test.registerCallback(someFunction);"
                     "test.registerCallback(someOtherFunction);"));
  ASSERT_TRUE(interface_owner_first.IsSet());
  ASSERT_TRUE(interface_owner_second.IsSet());

  EXPECT_FALSE(interface_owner_first.reference().referenced_value().EqualTo(
      interface_owner_second.reference().referenced_value()));
}

TEST_F(CallbackInterfaceTest, ObjectsAreEqual) {
  std::string result;

  InterfaceOwner interface_owner_first(&test_mock());
  InterfaceOwner interface_owner_second(&test_mock());
  EXPECT_CALL(test_mock(), RegisterCallback(_))
      .Times(2)
      .WillOnce(Invoke(&interface_owner_first, &InterfaceOwner::TakeOwnership))
      .WillOnce(
          Invoke(&interface_owner_second, &InterfaceOwner::TakeOwnership));

  EXPECT_TRUE(
      EvaluateScript("var someObject = { };"
                     "test.registerCallback(someObject);"
                     "test.registerCallback(someObject);"));
  ASSERT_TRUE(interface_owner_first.IsSet());
  ASSERT_TRUE(interface_owner_second.IsSet());

  EXPECT_TRUE(interface_owner_first.reference().referenced_value().EqualTo(
      interface_owner_second.reference().referenced_value()));
}

TEST_F(CallbackInterfaceTest, ObjectsAreNotEqual) {
  std::string result;

  InterfaceOwner interface_owner_first(&test_mock());
  InterfaceOwner interface_owner_second(&test_mock());
  EXPECT_CALL(test_mock(), RegisterCallback(_))
      .Times(2)
      .WillOnce(Invoke(&interface_owner_first, &InterfaceOwner::TakeOwnership))
      .WillOnce(
          Invoke(&interface_owner_second, &InterfaceOwner::TakeOwnership));

  EXPECT_TRUE(
      EvaluateScript("var someObject = { };"
                     "var someOtherObject = { };"
                     "test.registerCallback(someObject);"
                     "test.registerCallback(someOtherObject);"));
  ASSERT_TRUE(interface_owner_first.IsSet());
  ASSERT_TRUE(interface_owner_second.IsSet());

  EXPECT_FALSE(interface_owner_first.reference().referenced_value().EqualTo(
      interface_owner_second.reference().referenced_value()));
}

TEST_F(CallbackInterfaceTest, GetAndSetAttribute) {
  std::string result;

  InterfaceOwner interface_owner(&test_mock());
  EXPECT_CALL(test_mock(), set_callback_attribute(_))
      .WillOnce(Invoke(&interface_owner, &InterfaceOwner::TakeOwnership));

  // Set the attribute.
  EXPECT_TRUE(
      EvaluateScript("var implementer = {};"
                     "test.callbackAttribute = implementer;"));
  ASSERT_TRUE(interface_owner.IsSet());

  // Get the attribute value and check that it's equal.
  EXPECT_CALL(test_mock(), callback_attribute())
      .WillOnce(Return(&interface_owner.reference().referenced_value()));
  EXPECT_TRUE(
      EvaluateScript("test.callbackAttribute === implementer;", &result));
  EXPECT_STREQ("true", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/bindings/testing/base_interface.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/derived_interface.h"
#include "cobalt/bindings/testing/object_type_bindings_interface.h"
#include "cobalt/bindings/testing/script_object_owner.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ContainsRegex;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
class PlatformObjectBindingsTest
    : public InterfaceBindingsTest<ObjectTypeBindingsInterface> {
 public:
  PlatformObjectBindingsTest()
      : arbitrary_interface_(new StrictMock<ArbitraryInterface>()),
        base_interface_(new StrictMock<BaseInterface>()),
        derived_interface_(new StrictMock<DerivedInterface>()) {
    ON_CALL(test_mock(), arbitrary_object())
        .WillByDefault(Return(arbitrary_interface_));
    ON_CALL(test_mock(), base_interface())
        .WillByDefault(Return(base_interface_));
    ON_CALL(test_mock(), derived_interface())
        .WillByDefault(Return(derived_interface_));
  }

 protected:
  const scoped_refptr<ArbitraryInterface> arbitrary_interface_;
  const scoped_refptr<BaseInterface> base_interface_;
  const scoped_refptr<DerivedInterface> derived_interface_;
};

class UserObjectBindingsTest
    : public InterfaceBindingsTest<ObjectTypeBindingsInterface> {
 public:
  UserObjectBindingsTest()
      : arbitrary_interface_(new StrictMock<ArbitraryInterface>()) {
    ON_CALL(test_mock(), arbitrary_object())
        .WillByDefault(Return(arbitrary_interface_));
  }

 protected:
  const scoped_refptr<ArbitraryInterface> arbitrary_interface_;
};
}  // namespace

TEST_F(PlatformObjectBindingsTest, InstanceOfObject) {
  EXPECT_CALL(test_mock(), arbitrary_object());

  std::string result;
  EXPECT_TRUE(
      EvaluateScript("test.arbitraryObject instanceof Object;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(PlatformObjectBindingsTest, Prototype) {
  EXPECT_CALL(test_mock(), arbitrary_object());

  std::string result;
  EXPECT_TRUE(
      EvaluateScript("Object.getPrototypeOf(test.arbitraryObject);", &result));
  EXPECT_STREQ("[object ArbitraryInterfacePrototype]", result.c_str());
}

#if defined(ENGINE_DEFINES_ATTRIBUTES_ON_OBJECT)
TEST_F(PlatformObjectBindingsTest, PropertyIsOwnProperty) {
  EXPECT_CALL(test_mock(), arbitrary_object());

  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "test.arbitraryObject.hasOwnProperty(\"arbitraryProperty\");", &result));
  EXPECT_STREQ("true", result.c_str());
}
#else
TEST_F(PlatformObjectBindingsTest, PropertyIsDefinedOnPrototype) {
  EXPECT_CALL(test_mock(), arbitrary_object());

  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(test.arbitraryObject).hasOwnProperty("
      "\"arbitraryProperty\");",
      &result));
  EXPECT_STREQ("true", result.c_str());
}
#endif  // defined(ENGINE_DEFINES_ATTRIBUTES_ON_OBJECT)

TEST_F(PlatformObjectBindingsTest, MemberFunctionIsPrototypeProperty) {
  EXPECT_CALL(test_mock(), arbitrary_object()).Times(3);

  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "test.arbitraryObject.hasOwnProperty(\"arbitraryFunction\");", &result));
  EXPECT_STREQ("false", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(test.arbitraryObject).hasOwnProperty("
      "\"arbitraryFunction\");",
      &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_TRUE(EvaluateScript("\"arbitraryFunction\" in test.arbitraryObject;",
                             &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(PlatformObjectBindingsTest, DerivedProto) {
  EXPECT_CALL(test_mock(), derived_interface());

  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(test.derivedInterface) === "
      "DerivedInterface.prototype;",
      &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(PlatformObjectBindingsTest, PropertyOnBaseClass) {
  EXPECT_CALL(test_mock(), derived_interface());
  EXPECT_CALL(*derived_interface_, base_attribute())
      .WillOnce(Return("mock_value"));

  std::string result;
  EXPECT_TRUE(EvaluateScript("test.derivedInterface.baseAttribute;", &result));
  EXPECT_STREQ("mock_value", result.c_str());
}

TEST_F(PlatformObjectBindingsTest, OperationOnBaseClass) {
  EXPECT_CALL(test_mock(), derived_interface());
  EXPECT_CALL(*derived_interface_, BaseOperation());

  EXPECT_TRUE(EvaluateScript("test.derivedInterface.baseOperation();", NULL));
}

TEST_F(PlatformObjectBindingsTest, PropertyOnDerivedClass) {
  EXPECT_CALL(test_mock(), derived_interface());
  EXPECT_CALL(*derived_interface_, derived_attribute())
      .WillOnce(Return("mock_value"));

  std::string result;
  EXPECT_TRUE(
      EvaluateScript("test.derivedInterface.derivedAttribute;", &result));
  EXPECT_STREQ("mock_value", result.c_str());
}

TEST_F(PlatformObjectBindingsTest, OperationOnDerivedClass) {
  EXPECT_CALL(test_mock(), derived_interface());
  EXPECT_CALL(*derived_interface_, DerivedOperation());

  EXPECT_TRUE(
      EvaluateScript("test.derivedInterface.derivedOperation();", NULL));
}

TEST_F(PlatformObjectBindingsTest, ReturnDerivedClassWrapper) {
  ON_CALL(test_mock(), base_interface())
      .WillByDefault(Return(derived_interface_));

  std::string result;
  EXPECT_CALL(test_mock(), base_interface());
  EXPECT_TRUE(
      EvaluateScript("Object.getPrototypeOf(test.baseInterface);", &result));
  EXPECT_STREQ("[object DerivedInterfacePrototype]", result.c_str());

  EXPECT_CALL(test_mock(), base_interface());
  EXPECT_CALL(*derived_interface_, DerivedOperation());
  EXPECT_TRUE(EvaluateScript("test.baseInterface.derivedOperation();", NULL));
}

TEST_F(PlatformObjectBindingsTest, ReturnCorrectWrapperForObjectType) {
  typedef ScriptObjectOwner<script::ValueHandleHolder> ObjectOwner;
  ObjectOwner object_owner(&test_mock());

  // Store the Wrappable object as an opaque handle.
  EXPECT_CALL(test_mock(), set_object_property(_))
      .WillOnce(Invoke(&object_owner, &ObjectOwner::TakeOwnership));
  EXPECT_TRUE(
      EvaluateScript("test.objectProperty = new ArbitraryInterface();", NULL));
  ASSERT_TRUE(object_owner.IsSet());

  // Return it back to JS as the original type.
  EXPECT_CALL(test_mock(), object_property())
      .WillOnce(Return(&object_owner.reference().referenced_value()));
  EXPECT_TRUE(
      EvaluateScript("Object.getPrototypeOf(test.objectProperty) === "
                     "ArbitraryInterface.prototype;",
                     NULL));
}

TEST_F(UserObjectBindingsTest, PassUserObjectforObjectType) {
  typedef ScriptObjectOwner<script::ValueHandleHolder> ObjectOwner;
  ObjectOwner object_owner(&test_mock());

  EXPECT_TRUE(EvaluateScript("var obj = new Object();", NULL));
  EXPECT_CALL(test_mock(), set_object_property(_))
      .WillOnce(Invoke(&object_owner, &ObjectOwner::TakeOwnership));
  EXPECT_TRUE(EvaluateScript("test.objectProperty = obj;", NULL));
  ASSERT_TRUE(object_owner.IsSet());

  EXPECT_CALL(test_mock(), object_property())
      .WillOnce(Return(&object_owner.reference().referenced_value()));
  EXPECT_TRUE(EvaluateScript("test.objectProperty === obj;", NULL));
}

TEST_F(UserObjectBindingsTest, NullObject) {
  typedef ScriptObjectOwner<script::ValueHandleHolder> ObjectOwner;
  ObjectOwner object_owner(&test_mock());

  EXPECT_CALL(test_mock(), set_object_property(_))
      .WillOnce(Invoke(&object_owner, &ObjectOwner::TakeOwnership));
  EXPECT_TRUE(EvaluateScript("test.objectProperty = null;", NULL));
  EXPECT_FALSE(object_owner.IsSet());

  EXPECT_CALL(test_mock(), object_property());
  EXPECT_TRUE(EvaluateScript("test.objectProperty === null;", NULL));
}

TEST_F(UserObjectBindingsTest, NonObjectType) {
  std::string result;
  EXPECT_FALSE(EvaluateScript("test.objectProperty = 5;", &result));
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.objectProperty = false;", &result));
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.objectProperty = \"string\";", &result));
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(EvaluateScript("test.objectProperty = undefined;", &result));
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));
}

TEST_F(UserObjectBindingsTest, SetNonObject) {
  std::string result;
  EXPECT_FALSE(EvaluateScript("test.objectProperty = 5", &result));
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));
}

TEST_F(UserObjectBindingsTest, SetUndefinedObject) {
  std::string result;
  EXPECT_FALSE(EvaluateScript("test.arbitraryObject = undefined;", &result));
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));
}

TEST_F(UserObjectBindingsTest, SetWrongObjectType) {
  std::string result;
  EXPECT_FALSE(EvaluateScript("test.arbitraryObject = new Object()", &result));
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));

  EXPECT_FALSE(
      EvaluateScript("test.derivedInterface = new BaseInterface()", &result));
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));
}

TEST_F(UserObjectBindingsTest, CallWrongObjectType) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("var obj = new Object()", NULL));
  EXPECT_TRUE(EvaluateScript("var arb = new ArbitraryInterface()", NULL));

  // Calling a function with the wrong object type is a type error.
  EXPECT_FALSE(
      EvaluateScript("obj.arbitraryFunction = arb.arbitraryFunction;\n"
                     "obj.arbitraryFunction();", &result));
  EXPECT_THAT(result.c_str(), ContainsRegex("TypeError:"));
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

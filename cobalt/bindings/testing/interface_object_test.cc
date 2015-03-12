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
#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/bindings/testing/no_interface_object_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef BindingsTestBase<ArbitraryInterface> InterfaceObjectTest;
typedef BindingsTestBase<NoInterfaceObjectInterface> NoInterfaceObjectTest;
}  // namespace

// Spec for interface object: http://www.w3.org/TR/WebIDL/#interface-object
// Spec for interface prototype object:
//     http://www.w3.org/TR/WebIDL/#interface-prototype-object

// Interface object for non-callback interface is a function object.
TEST_F(InterfaceObjectTest, DISABLED_InterfaceObjectIsGlobalProperty) {
  // TODO(***REMOVED***): This is dependent on generating bindings for the
  //     Global Object
  NOTREACHED();
}

// Interface object for non-callback interface is a function object.
TEST_F(InterfaceObjectTest, InterfaceObjectIsFunctionObject) {
  // TODO(***REMOVED***): Once the Interface Object is exposed on the global
  // object, get it from there directly rather than through the prototype.
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(test).constructor instanceof Function;", &result));
  EXPECT_STREQ("true", result.c_str());
}

// The value of the "prototype" property must be the
// 'interface prototype object'.
TEST_F(InterfaceObjectTest, PrototypePropertyIsSet) {
  // TODO(***REMOVED***): Once the Interface Object is exposed on the global
  // object, get it from there directly rather than through the prototype.
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(test).constructor.hasOwnProperty(\"prototype\");",
      &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(test).constructor.prototype == "
      "Object.getPrototypeOf(test);",
      &result));
  EXPECT_STREQ("true", result.c_str());
}

// The interface prototype object must have a property named "constructor" whose
// value is a reference the the interface object.
TEST_F(InterfaceObjectTest, ConstructorPropertyExists) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("test.hasOwnProperty(\"constructor\");", &result));
  EXPECT_STREQ("false", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(test).hasOwnProperty(\"constructor\");", &result));
  EXPECT_STREQ("true", result.c_str());
}

// The interface prototype object must have a property named "constructor" whose
// value is a reference the the interface object.
TEST_F(InterfaceObjectTest, DISABLED_ConstructorPropertyIsInterfaceObject) {
  // TODO(***REMOVED***): This is dependent on the Interface Object being bound to
  //     a property on the Global Object.
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(test).constructor === ArbitraryInterface;",
      &result));
  EXPECT_STREQ("true", result.c_str());
}

// If [NoInterfaceObject] extended attribute is specified, there should be no
// constructor property on the prototype.
TEST_F(NoInterfaceObjectTest, NoConstructorProperty) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(test).hasOwnProperty(\"constructor\");", &result));
  EXPECT_STREQ("false", result.c_str());
}

// Interface object for non-callback interface is a function object.
TEST_F(NoInterfaceObjectTest, NoInterfaceObjectGlobalProperty) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "this.hasOwnProperty(\"NoInterfaceObjectInterface\");", &result));
  EXPECT_STREQ("false", result.c_str());
}


}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

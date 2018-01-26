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

#include "cobalt/bindings/testing/arbitrary_interface.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/no_interface_object_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<ArbitraryInterface> InterfaceObjectTest;
typedef InterfaceBindingsTest<NoInterfaceObjectInterface> NoInterfaceObjectTest;
}  // namespace

// Spec for interface object: https://www.w3.org/TR/WebIDL/#interface-object
// Spec for interface prototype object:
//     https://www.w3.org/TR/WebIDL/#interface-prototype-object

// Interface object for non-callback interface is a global property.
TEST_F(InterfaceObjectTest, InterfaceObjectIsGlobalProperty) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "this.hasOwnProperty(\"ArbitraryInterface\");", &result));
  EXPECT_STREQ("true", result.c_str());
}

// Interface object for non-callback interface is a function object.
TEST_F(InterfaceObjectTest, InterfaceObjectIsFunctionObject) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "ArbitraryInterface instanceof Function;", &result));
  EXPECT_STREQ("true", result.c_str());
}

// The value of the "prototype" property must be the
// 'interface prototype object'.
TEST_F(InterfaceObjectTest, PrototypePropertyIsSet) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "ArbitraryInterface.hasOwnProperty(\"prototype\");", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "ArbitraryInterface.prototype == Object.getPrototypeOf(test);",
      &result));
  EXPECT_STREQ("true", result.c_str());
}

// The value of the "name" property must be the identifier of the interface.
TEST_F(InterfaceObjectTest, NamePropertyIsSet) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("ArbitraryInterface.name;", &result));
  EXPECT_STREQ("ArbitraryInterface", result.c_str());
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
TEST_F(InterfaceObjectTest, ConstructorPropertyIsInterfaceObject) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(test).constructor === ArbitraryInterface;",
      &result));
  EXPECT_STREQ("true", result.c_str());
}

// Interface object for non-callback interface is a function object.
TEST_F(NoInterfaceObjectTest, NoInterfaceObjectGlobalProperty) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "this.hasOwnProperty(\"NoInterfaceObjectInterface\");", &result));
  EXPECT_STREQ("false", result.c_str());
}

TEST_F(InterfaceObjectTest, FunctionCanHandleAllJavaScriptValueTypes) {
  const char* script = R"EOF(
      const f = ArbitraryInterface.prototype.ArbitraryFunction;
      [null, undefined, false, 0, "", {}, Symbol("")]
        .map(value => {
          try { f.call(value); }
          catch (ex) { return ex.toString().startsWith("TypeError"); }
          return false;
        })
        .every(result => result);
  )EOF";
  std::string result;
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_STREQ("true", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

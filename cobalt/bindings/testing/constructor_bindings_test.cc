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

#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/constructor_interface.h"
#include "cobalt/bindings/testing/named_constructor_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace cobalt {
namespace bindings {
namespace testing {
namespace {
typedef BindingsTestBase ConstructorBindingsTest;
}  // namespace

TEST_F(ConstructorBindingsTest, CanConstructNewObject) {
  EXPECT_CALL(ConstructorInterface::constructor_implementation_mock.Get(),
              Constructor());
  EXPECT_TRUE(EvaluateScript("var obj = new ConstructorInterface()", NULL));

  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(obj).constructor === ConstructorInterface",
      &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(ConstructorBindingsTest, CanConstructNewObjectWithArguments) {
  EXPECT_TRUE(EvaluateScript(
      "var obj = new ConstructorWithArgumentsInterface(4, false)", NULL));

  std::string result;
  EXPECT_TRUE(EvaluateScript("obj.longArg", &result));
  EXPECT_STREQ("4", result.c_str());
  EXPECT_TRUE(EvaluateScript("obj.booleanArg", &result));
  EXPECT_STREQ("false", result.c_str());
  EXPECT_TRUE(EvaluateScript("obj.stringArg", &result));
  EXPECT_STREQ("default", result.c_str());

  EXPECT_TRUE(EvaluateScript(
      "var obj = new ConstructorWithArgumentsInterface(-5, true, \"foo\")",
      NULL));
  EXPECT_TRUE(EvaluateScript("obj.stringArg", &result));
  EXPECT_STREQ("foo", result.c_str());
}

// Length property is set to the length of the shortest argument list of all
// overload sets.
TEST_F(ConstructorBindingsTest, LengthPropertyIsSet) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("ConstructorInterface.length == 0", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_TRUE(EvaluateScript("NoConstructorInterface.length == 0", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("ConstructorWithArgumentsInterface.length == 2", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(ConstructorBindingsTest, OverloadedConstructor) {
  EXPECT_CALL(ConstructorInterface::constructor_implementation_mock.Get(),
              Constructor());
  EXPECT_TRUE(EvaluateScript("var obj = new ConstructorInterface()", NULL));

  EXPECT_CALL(ConstructorInterface::constructor_implementation_mock.Get(),
              Constructor(_));
  EXPECT_TRUE(EvaluateScript("var obj = new ConstructorInterface(true)", NULL));
}

TEST_F(ConstructorBindingsTest, NamedConstructor) {
  EXPECT_CALL(NamedConstructorInterface::constructor_implementation_mock.Get(),
              Constructor());
  EXPECT_TRUE(EvaluateScript("var obj = new SomeNamedConstructor()", NULL));
}

TEST_F(ConstructorBindingsTest, NamedConstructorHasCorrectName) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("SomeNamedConstructor.name;", &result));
  EXPECT_STREQ("SomeNamedConstructor", result.c_str());
}

TEST_F(ConstructorBindingsTest, NamedConstructorHasCorrectPrototype) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(SomeNamedConstructor) === "
      "Object.getPrototypeOf(NamedConstructorInterface);",
      &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(ConstructorBindingsTest, IllegalConstructorThrows) {
  const char script[] = R"(
      let result = null;
      try {
        const ngi = new NamedGetterInterface();
        // Poke at the object a little bit in order to ensure it is actually
        // backed by a native object.
        ngi.property;
      } catch (ex) {
        result = ex.name;
      }
      result;
  )";
  std::string result;
  EXPECT_TRUE(EvaluateScript(script, &result));
  EXPECT_STREQ("TypeError", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

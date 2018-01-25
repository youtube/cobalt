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

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/utils.h"
#include "cobalt/bindings/testing/window_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
class GlobalInterfaceBindingsTest : public BindingsTestBase {
 protected:
  GlobalInterfaceBindingsTest() : BindingsTestBase(new WindowMock()) {}
  WindowMock& test_mock() {
    return *base::polymorphic_downcast<WindowMock*>(window_.get());
  }
};
}  // namespace

TEST_F(GlobalInterfaceBindingsTest, GlobalWindowIsThis) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("window === this;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(GlobalInterfaceBindingsTest, GlobalOperation) {
  EXPECT_CALL(test_mock(), WindowOperation());
  EXPECT_TRUE(EvaluateScript("windowOperation();", NULL));
}

TEST_F(GlobalInterfaceBindingsTest, GlobalProperty) {
  InSequence in_sequence_dummy;

  std::string result;
  EXPECT_CALL(test_mock(), window_property()).WillOnce(Return("bar"));
  EXPECT_TRUE(EvaluateScript("windowProperty == \"bar\";", &result));
  EXPECT_STREQ("true", result.c_str());
  EXPECT_CALL(test_mock(), set_window_property("foo"));
  EXPECT_TRUE(EvaluateScript("windowProperty = \"foo\";", NULL));
}

TEST_F(GlobalInterfaceBindingsTest, GlobalPrototypeIsSet) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(this) === Window.prototype;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(GlobalInterfaceBindingsTest, GlobalInterfaceConstructorIsSet) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("this.constructor === Window;", &result));
  EXPECT_STREQ("true", result.c_str());
}

// http://heycam.github.io/webidl/#Global
//   2. Interface members from the interface (or consequential interfaces) will
//      correspond to properties on the object itself rather than on interface
//      prototype objects.
TEST_F(GlobalInterfaceBindingsTest, DISABLED_PropertiesAndOperationsAreOwnProperties) {
  std::string result;
  EXPECT_TRUE(
      EvaluateScript("this.hasOwnProperty(\"windowProperty\");", &result));
  EXPECT_STREQ("true", result.c_str());
  EXPECT_TRUE(
      EvaluateScript("this.hasOwnProperty(\"windowOperation\");", &result));
  EXPECT_STREQ("true", result.c_str());
  EXPECT_TRUE(EvaluateScript(
      "Window.prototype.hasOwnProperty(\"windowProperty\");", &result));
  EXPECT_STREQ("false", result.c_str());
  EXPECT_TRUE(EvaluateScript(
      "Window.prototype.hasOwnProperty(\"windowOperation\");", &result));
  EXPECT_STREQ("false", result.c_str());
}

TEST_F(GlobalInterfaceBindingsTest, ConstructorExists) {
  std::string result;
  EXPECT_TRUE(
      EvaluateScript("this.hasOwnProperty(\"ArbitraryInterface\");", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(GlobalInterfaceBindingsTest, ConstructorPrototype) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("ArbitraryInterface.prototype", &result));
  EXPECT_TRUE(IsAcceptablePrototypeString("ArbitraryInterface", result))
      << result;
}

TEST_F(GlobalInterfaceBindingsTest,
       ConstructorForNoInterfaceObjectDoesNotExists) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "this.hasOwnProperty(\"NoInterfaceObjectInterface\");", &result));
  EXPECT_STREQ("false", result.c_str());
}

TEST_F(GlobalInterfaceBindingsTest, CallParentInterfaceOperation) {
  EXPECT_CALL(test_mock(), ParentOperation());
  EXPECT_TRUE(EvaluateScript("parentOperation();", NULL));
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

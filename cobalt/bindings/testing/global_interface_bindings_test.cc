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

#include "cobalt/bindings/testing/test_window_mock.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/global_object_proxy.h"
#include "cobalt/script/source_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {

class GlobalInterfaceBindingsTest : public ::testing::Test {
 public:
  GlobalInterfaceBindingsTest()
      : engine_(script::JavaScriptEngine::CreateEngine()),
        // Use StrictMock so TESTING will fail if unexpected method is called.
        window_mock_(new ::testing::StrictMock<TestWindowMock>()),
        global_object_proxy_(engine_->CreateGlobalObject()) {
    global_object_proxy_->SetGlobalInterface(
        make_scoped_refptr<TestWindow>(window_mock_));
  }

  bool EvaluateScript(const std::string& script, std::string* out_result) {
    scoped_refptr<script::SourceCode> source =
        script::SourceCode::CreateSourceCode(script);
    return global_object_proxy_->EvaluateScript(source, out_result);
  }
  TestWindowMock& test_mock() { return *window_mock_.get(); }

 private:
  const scoped_ptr<script::JavaScriptEngine> engine_;
  const scoped_refptr<TestWindowMock> window_mock_;
  const scoped_refptr<script::GlobalObjectProxy> global_object_proxy_;
};
}  // namespace

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
      "Object.getPrototypeOf(this) === TestWindow.prototype;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(GlobalInterfaceBindingsTest, GlobalInterfaceConstructorIsSet) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("this.constructor === TestWindow;", &result));
  EXPECT_STREQ("true", result.c_str());
}

// http://heycam.github.io/webidl/#Global
//   2. Interface members from the interface (or consequential interfaces) will
//      correspond to properties on the object itself rather than on interface
//      prototype objects.
TEST_F(GlobalInterfaceBindingsTest, PropertiesAndOperationsAreOwnProperties) {
  std::string result;
  EXPECT_TRUE(
      EvaluateScript("this.hasOwnProperty(\"windowProperty\");", &result));
  EXPECT_STREQ("true", result.c_str());
  EXPECT_TRUE(
      EvaluateScript("this.hasOwnProperty(\"windowOperation\");", &result));
  EXPECT_STREQ("true", result.c_str());
  EXPECT_TRUE(EvaluateScript(
      "TestWindow.prototype.hasOwnProperty(\"windowProperty\");", &result));
  EXPECT_STREQ("false", result.c_str());
  EXPECT_TRUE(EvaluateScript(
      "TestWindow.prototype.hasOwnProperty(\"windowOperation\");", &result));
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
  EXPECT_STREQ("[object ArbitraryInterfacePrototype]", result.c_str());
}

TEST_F(GlobalInterfaceBindingsTest,
       ConstructorForNoInterfaceObjectDoesNotExists) {
  std::string result;
  EXPECT_TRUE(EvaluateScript(
      "this.hasOwnProperty(\"NoInterfaceObjectInterface\");", &result));
  EXPECT_STREQ("false", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

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

#include <limits>

#include "base/strings/stringprintf.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/extended_idl_attributes_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<ExtendedIDLAttributesInterface>
    ExtendedAttributesTest;
}  // namespace

TEST_F(ExtendedAttributesTest, CallWithEnvironmentSettings) {
  EXPECT_CALL(test_mock(), CallWithSettings(environment_settings_.get()));
  EXPECT_TRUE(EvaluateScript("test.callWithSettings();", NULL));
}

TEST_F(ExtendedAttributesTest, ClampArgument) {
  EXPECT_CALL(test_mock(), ClampArgument(std::numeric_limits<uint16_t>::max()));
  EXPECT_TRUE(
      EvaluateScript(base::StringPrintf("test.clampArgument(%u);",
                                        std::numeric_limits<uint32_t>::max()),
                     NULL));
}

TEST_F(ExtendedAttributesTest, ImplementedAsAttributeGetter) {
  EXPECT_CALL(test_mock(), attribute_default()).WillOnce(Return(true));
  EXPECT_TRUE(EvaluateScript("var result = test.default;", NULL));

  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof result;", &result));
  EXPECT_STREQ("boolean", result.c_str());

  EXPECT_TRUE(EvaluateScript("result;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), attribute_default()).WillOnce(Return(false));
  EXPECT_TRUE(EvaluateScript("var result = test.default;", NULL));

  EXPECT_TRUE(EvaluateScript("typeof result;", &result));
  EXPECT_STREQ("boolean", result.c_str());

  EXPECT_TRUE(EvaluateScript("result;", &result));
  EXPECT_STREQ("false", result.c_str());
}

TEST_F(ExtendedAttributesTest, ImplementedAsAttributeSetter) {
  EXPECT_CALL(test_mock(), set_attribute_default(true));
  EXPECT_TRUE(EvaluateScript(base::StringPrintf("test.default = true;"), NULL));

  EXPECT_CALL(test_mock(), set_attribute_default(false));
  EXPECT_TRUE(
      EvaluateScript(base::StringPrintf("test.default = false;"), NULL));
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

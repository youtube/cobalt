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

#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/interface_with_unsupported_properties.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef BindingsTestBase UnsupportedInterfaceTest;
typedef InterfaceBindingsTest<InterfaceWithUnsupportedProperties>
    InterfaceWithUnsupportedPropertiesTest;
}  // namespace

// Current unsupported property detection causes these to incorrectly reporting
// that these properties exist.
TEST_F(InterfaceWithUnsupportedPropertiesTest,
       DISABLED_DoesNotHaveUnsupportedProperty) {
  std::string result;

  EXPECT_TRUE(EvaluateScript("\"unsupportedAttribute\" in test;", &result));
  EXPECT_STREQ("false", result.c_str());

  EXPECT_TRUE(EvaluateScript("\"unsupportedOperation\" in test;", &result));
  EXPECT_STREQ("false", result.c_str());

  EXPECT_TRUE(EvaluateScript("\"UNSUPPORTED_CONSTANT\" in test;", &result));
  EXPECT_STREQ("false", result.c_str());

  EXPECT_TRUE(
      EvaluateScript("\"UNSUPPORTED_CONSTANT\" in test.constructor;", &result));
  EXPECT_STREQ("false", result.c_str());

  EXPECT_TRUE(EvaluateScript("\"supportedAttribute\" in test;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(UnsupportedInterfaceTest, InterfaceObjectDoesNotExist) {
  std::string result;
  EXPECT_TRUE(EvaluateScript("\"UnsupportedInterface\" in this;", &result));
  EXPECT_STREQ("false", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

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

#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/conditional_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<ConditionalInterface> ConditionalInterfaceTest;
typedef BindingsTestBase DisabledInterfaceTest;
}  // namespace

TEST_F(ConditionalInterfaceTest, HasInterfaceObject) {
  EXPECT_TRUE(
      EvaluateScript("this.hasOwnProperty(\"ConditionalInterface\");", NULL));
}

TEST_F(ConditionalInterfaceTest, HasConditionalProperties) {
  EXPECT_TRUE(EvaluateScript(
      "ConditionalInterface.hasOwnProperty(\"enabledOperation\");", NULL));
  EXPECT_TRUE(
      EvaluateScript("test.hasOwnProperty(\"enabledAttribute\");", NULL));
}

TEST_F(ConditionalInterfaceTest, DoesNotHaveDisabledProperties) {
  EXPECT_TRUE(EvaluateScript(
      "!ConditionalInterface.hasOwnProperty(\"disabledOperation\");", NULL));
  EXPECT_TRUE(
      EvaluateScript("!test.hasOwnProperty(\"disabledAttribute\");", NULL));
}

TEST_F(DisabledInterfaceTest, NoInterfaceObject) {
  EXPECT_TRUE(EvaluateScript("this.DisabledInterface === undefined", NULL));
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

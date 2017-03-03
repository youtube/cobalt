// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<ArbitraryInterface> GetOwnPropertyDescriptorTest;
}  // namespace

// TODO:  This test is expected to succeed on SpiderMonkey 24 and fail on
// SpiderMonkey 45 due to an improper proxy handler implementation.  See
// "cobalt/script/mozjs-45/proxy_handler.h" for more details.  The override of
// |getOwnPropertyDescriptor| should not call |getPropertyDescriptor|, however
// currently does as a temporary fix for other tests.
TEST_F(GetOwnPropertyDescriptorTest,
       DoesNotHaveArbitraryPropertyPropertyDescriptor) {
  std::string result;
  EvaluateScript("Object.getOwnPropertyDescriptor(test, \"arbitraryProperty\")",
                 &result);
  EXPECT_EQ(result, "undefined");
  EXPECT_CALL(test_mock(), arbitrary_property());
  EvaluateScript("test.arbitraryProperty", &result);
  EXPECT_EQ(result, "");
}

TEST_F(GetOwnPropertyDescriptorTest, GetPropertyDescriptorFromPrototype) {
  std::string result;
  EvaluateScript(
      "var descriptor = "
      "Object.getOwnPropertyDescriptor(ArbitraryInterface.prototype, "
      "\"arbitraryProperty\")",
      &result);
  EXPECT_EQ(result, "undefined");
  EvaluateScript("descriptor.configurable", &result);
  EXPECT_EQ(result, "true");
  EvaluateScript("descriptor.enumerable", &result);
  EXPECT_EQ(result, "true");
  EvaluateScript("descriptor.__proto__", &result);
  EXPECT_EQ(result, "[object Object]");
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

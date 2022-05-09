// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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
#include "cobalt/bindings/testing/bytestring_type_test_interface.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<BytestringTypeTestInterface>
    BytestringTypeBindingsTest;
}  // namespace

TEST_F(BytestringTypeBindingsTest, BytestringReturnValue) {
  EXPECT_TRUE(
      EvaluateScript("var result = test.bytestringReturnOperation();", NULL));

  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof result;", &result));
  EXPECT_STREQ("string", result.c_str());

  EXPECT_TRUE(EvaluateScript("result;", &result));
  EXPECT_STREQ("\001\002\003", result.c_str());
}

TEST_F(BytestringTypeBindingsTest, BytestringArgumentOperation) {
  EXPECT_TRUE(EvaluateScript(
      "var result = test.bytestringArgumentOperation('byteString');", NULL));

  std::string result;
  EXPECT_TRUE(EvaluateScript("typeof result;", &result));
  EXPECT_STREQ("string", result.c_str());

  EXPECT_TRUE(EvaluateScript("result;", &result));
  EXPECT_STREQ("byteString", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

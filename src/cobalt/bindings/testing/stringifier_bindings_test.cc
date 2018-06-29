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

#include <string>

#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/stringifier_anonymous_operation_interface.h"
#include "cobalt/bindings/testing/stringifier_attribute_interface.h"
#include "cobalt/bindings/testing/stringifier_operation_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<StringifierAttributeInterface>
    StringifierAttributeBindingsTest;
typedef InterfaceBindingsTest<StringifierOperationInterface>
    StringifierOperationBindingsTest;
typedef InterfaceBindingsTest<StringifierAnonymousOperationInterface>
    StringifierAnonymousOperationBindingsTest;
}  // namespace

TEST_F(StringifierAttributeBindingsTest, CallsGetter) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), the_stringifier_attribute())
      .Times(2)
      .WillRepeatedly(Return("foo"));
  EXPECT_TRUE(EvaluateScript("test.toString();", &result));
  EXPECT_STREQ("foo", result.c_str());
  EXPECT_TRUE(EvaluateScript("String(test);", &result));
  EXPECT_STREQ("foo", result.c_str());
}

TEST_F(StringifierOperationBindingsTest, CallsFunction) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), TheStringifierOperation())
      .Times(2)
      .WillRepeatedly(Return("foo"));
  EXPECT_TRUE(EvaluateScript("test.toString();", &result));
  EXPECT_STREQ("foo", result.c_str());
  EXPECT_TRUE(EvaluateScript("String(test);", &result));
  EXPECT_STREQ("foo", result.c_str());
}

TEST_F(StringifierAnonymousOperationBindingsTest, CallsFunction) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), AnonymousStringifier())
      .Times(2)
      .WillRepeatedly(Return("foo"));
  EXPECT_TRUE(EvaluateScript("test.toString();", &result));
  EXPECT_STREQ("foo", result.c_str());
  EXPECT_TRUE(EvaluateScript("String(test);", &result));
  EXPECT_STREQ("foo", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

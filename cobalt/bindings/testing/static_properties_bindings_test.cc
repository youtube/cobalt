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
#include "cobalt/bindings/testing/static_properties_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::A;
using ::testing::InSequence;
using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
namespace {
typedef BindingsTestBase StaticPropertiesBindingsTest;
}  // namespace
}  // namespace

TEST_F(StaticPropertiesBindingsTest, StaticFunction) {
  EXPECT_CALL(StaticPropertiesInterface::static_methods_mock.Get(),
              StaticFunction());
  EXPECT_TRUE(
      EvaluateScript("StaticPropertiesInterface.staticFunction();", NULL));
}

TEST_F(StaticPropertiesBindingsTest,
       StaticOverloadedOperationByNumberOfArguments) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(StaticPropertiesInterface::static_methods_mock.Get(),
              StaticFunction());
  EXPECT_TRUE(
      EvaluateScript("StaticPropertiesInterface.staticFunction();", NULL));

  EXPECT_CALL(StaticPropertiesInterface::static_methods_mock.Get(),
              StaticFunction(_, _, A<int32_t>()));
  EXPECT_TRUE(EvaluateScript(
      "StaticPropertiesInterface.staticFunction(6, 8, 9);", NULL));

  EXPECT_CALL(
      StaticPropertiesInterface::static_methods_mock.Get(),
      StaticFunction(_, _, A<const scoped_refptr<ArbitraryInterface>&>()));
  EXPECT_TRUE(
      EvaluateScript("StaticPropertiesInterface.staticFunction(6, 8, new "
                     "ArbitraryInterface());",
                     NULL));
}

TEST_F(StaticPropertiesBindingsTest, StaticOverloadedOperationByType) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(StaticPropertiesInterface::static_methods_mock.Get(),
              StaticFunction(A<int32_t>()));
  EXPECT_TRUE(
      EvaluateScript("StaticPropertiesInterface.staticFunction(4);", NULL));

  EXPECT_CALL(StaticPropertiesInterface::static_methods_mock.Get(),
              StaticFunction(A<const std::string&>()));
  EXPECT_TRUE(EvaluateScript(
      "StaticPropertiesInterface.staticFunction(\"foo\");", NULL));
}

TEST_F(StaticPropertiesBindingsTest, GetStaticAttribute) {
  std::string result;
  EXPECT_CALL(StaticPropertiesInterface::static_methods_mock.Get(),
              static_attribute()).WillOnce(Return("test_string"));
  EXPECT_TRUE(
      EvaluateScript("StaticPropertiesInterface.staticAttribute;", &result));
  EXPECT_STREQ("test_string", result.c_str());
}

TEST_F(StaticPropertiesBindingsTest, SetStaticAttribute) {
  EXPECT_CALL(StaticPropertiesInterface::static_methods_mock.Get(),
              set_static_attribute(std::string("test_string")));
  EXPECT_TRUE(EvaluateScript(
      "StaticPropertiesInterface.staticAttribute = \"test_string\";", NULL));
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

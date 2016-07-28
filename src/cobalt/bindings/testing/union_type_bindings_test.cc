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

#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/union_types_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::_;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<UnionTypesInterface> UnionTypesBindingsTest;
}  // namespace

TEST_F(UnionTypesBindingsTest, ConvertToJS) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), union_property()).WillOnce(
      Return(UnionTypesInterface::UnionPropertyType(std::string("foo"))));
  EXPECT_TRUE(
      EvaluateScript("typeof test.unionProperty == \"string\";", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), union_property())
      .WillOnce(Return(UnionTypesInterface::UnionPropertyType(
          make_scoped_refptr(new ArbitraryInterface()))));
  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(test.unionProperty) === "
      "ArbitraryInterface.prototype;",
      &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), union_property())
      .WillOnce(Return(UnionTypesInterface::UnionPropertyType(5)));
  EXPECT_TRUE(
      EvaluateScript("typeof test.unionProperty == \"number\";", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), union_property())
      .WillOnce(Return(UnionTypesInterface::UnionPropertyType(true)));
  EXPECT_TRUE(
      EvaluateScript("typeof test.unionProperty == \"boolean\";", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(UnionTypesBindingsTest, ConvertFromJS) {
  InSequence dummy;

  UnionTypesInterface::UnionPropertyType union_type;
  EXPECT_CALL(test_mock(), set_union_property(_))
      .WillOnce(SaveArg<0>(&union_type));
  EXPECT_TRUE(EvaluateScript("test.unionProperty = \"string type\";", NULL));
  ASSERT_TRUE(union_type.IsType<std::string>());
  EXPECT_STREQ("string type", union_type.AsType<std::string>().c_str());

  EXPECT_CALL(test_mock(), set_union_property(_))
      .WillOnce(SaveArg<0>(&union_type));
  EXPECT_TRUE(EvaluateScript("test.unionProperty = true;", NULL));
  ASSERT_TRUE(union_type.IsType<bool>());
  EXPECT_EQ(true, union_type.AsType<bool>());

  EXPECT_CALL(test_mock(), set_union_property(_))
      .WillOnce(SaveArg<0>(&union_type));
  EXPECT_TRUE(EvaluateScript("test.unionProperty = -17;", NULL));
  ASSERT_TRUE(union_type.IsType<int32_t>());
  EXPECT_EQ(-17, union_type.AsType<int32_t>());

  EXPECT_CALL(test_mock(), set_union_property(_))
      .WillOnce(SaveArg<0>(&union_type));
  EXPECT_TRUE(
      EvaluateScript("test.unionProperty = new ArbitraryInterface();", NULL));
  ASSERT_TRUE(union_type.IsType<scoped_refptr<ArbitraryInterface> >());
  EXPECT_TRUE(union_type.AsType<scoped_refptr<ArbitraryInterface> >());
}

TEST_F(UnionTypesBindingsTest, SetNullableUnion) {
  InSequence dummy;

  UnionTypesInterface::NullableUnionPropertyType union_type;
  EXPECT_CALL(test_mock(), set_union_with_nullable_member_property(_))
      .WillOnce(SaveArg<0>(&union_type));
  EXPECT_TRUE(
      EvaluateScript("test.unionWithNullableMemberProperty = null;", NULL));
  EXPECT_EQ(base::nullopt, union_type);

  EXPECT_CALL(test_mock(), set_nullable_union_property(_))
      .WillOnce(SaveArg<0>(&union_type));
  EXPECT_TRUE(EvaluateScript("test.nullableUnionProperty = null;", NULL));
  EXPECT_EQ(base::nullopt, union_type);
}

TEST_F(UnionTypesBindingsTest, GetNullableUnion) {
  InSequence dummy;

  std::string result;
  EXPECT_CALL(test_mock(), union_with_nullable_member_property())
      .WillOnce(Return(base::nullopt));
  EXPECT_TRUE(
      EvaluateScript("test.unionWithNullableMemberProperty == null;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), nullable_union_property())
      .WillOnce(Return(base::nullopt));
  EXPECT_TRUE(EvaluateScript("test.nullableUnionProperty == null;", &result));
  EXPECT_STREQ("true", result.c_str());
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

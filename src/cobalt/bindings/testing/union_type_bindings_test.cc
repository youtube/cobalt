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
#include "cobalt/bindings/testing/union_types_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ContainsRegex;
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
          base::WrapRefCounted(new ArbitraryInterface()))));
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

TEST_F(UnionTypesBindingsTest, ConvertFromJSInvalid) {
  InSequence dummy;

  UnionTypesInterface::UnionBasePropertyType union_base_type;
  // Try to assign wrong interface type.
  // First assign a valid value.
  EXPECT_CALL(test_mock(), set_union_base_property(_))
      .WillOnce(SaveArg<0>(&union_base_type));
  EXPECT_TRUE(
      EvaluateScript("test.unionBaseProperty = \"string type\";", NULL));
  ASSERT_TRUE(union_base_type.IsType<std::string>());
  EXPECT_EQ("string type", union_base_type.AsType<std::string>());
  // Attempt to assign invalid value.
  EXPECT_FALSE(EvaluateScript(
      "test.unionBaseProperty = new EnumerationInterface();", NULL));
  // Check the original value was preserved.
  ASSERT_TRUE(union_base_type.IsType<std::string>());
  EXPECT_EQ("string type", union_base_type.AsType<std::string>());
}

TEST_F(UnionTypesBindingsTest, ConvertFromJSInherit) {
  InSequence dummy;

  UnionTypesInterface::UnionBasePropertyType union_base_type;
  // Assign with base.
  EXPECT_CALL(test_mock(), set_union_base_property(_))
      .WillOnce(SaveArg<0>(&union_base_type));
  EXPECT_TRUE(
      EvaluateScript("test.unionBaseProperty = new BaseInterface();", NULL));
  ASSERT_TRUE(union_base_type.IsType<scoped_refptr<BaseInterface> >());
  EXPECT_TRUE(union_base_type.AsType<scoped_refptr<BaseInterface> >());

  // Assign with derived.
  EXPECT_CALL(test_mock(), set_union_base_property(_))
      .WillOnce(SaveArg<0>(&union_base_type));
  EXPECT_TRUE(
      EvaluateScript("test.unionBaseProperty = new DerivedInterface();", NULL));
  ASSERT_TRUE(union_base_type.IsType<scoped_refptr<BaseInterface> >());
  EXPECT_TRUE(union_base_type.AsType<scoped_refptr<BaseInterface> >());
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

TEST_F(UnionTypesBindingsTest, ConvertDictFromJS) {
  InSequence dummy;

  // Union can still accept a string
  UnionTypesInterface::UnionDictPropertyType property;
  EXPECT_CALL(test_mock(), set_union_with_dictionary_property(_))
      .WillOnce(SaveArg<0>(&property));
  EXPECT_TRUE(
      EvaluateScript("test.unionWithDictionaryProperty = 'foo';", NULL));
  ASSERT_TRUE(property.IsType<std::string>());
  EXPECT_EQ("foo", property.AsType<std::string>());

  // Dictionary is properly converted and populated
  EXPECT_CALL(test_mock(), set_union_with_dictionary_property(_))
      .WillOnce(SaveArg<0>(&property));
  EXPECT_TRUE(
      EvaluateScript("test.unionWithDictionaryProperty = "
                     "{ 'stringMember' : 'foo', 'longMember' : 42 };",
                     NULL));
  ASSERT_TRUE(property.IsType<TestDictionary>());
  EXPECT_TRUE(property.AsType<TestDictionary>().has_string_member());
  EXPECT_EQ(property.AsType<TestDictionary>().string_member(), "foo");
  EXPECT_TRUE(property.AsType<TestDictionary>().has_long_member());
  EXPECT_EQ(property.AsType<TestDictionary>().long_member(), 42);

  // Derived dictionary can be converted, and takes precedence over an object
  UnionTypesInterface::UnionObjectsPropertyType other_property;
  EXPECT_CALL(test_mock(), set_union_dicts_objects_property(_))
      .WillOnce(SaveArg<0>(&other_property));
  EXPECT_TRUE(
      EvaluateScript("test.unionDictsObjectsProperty = "
                     "{ 'additionalMember' : true, 'longMember' : 2 };",
                     NULL));
  ASSERT_TRUE(other_property.IsType<DerivedDictionary>());
  EXPECT_EQ(true,
            other_property.AsType<DerivedDictionary>().additional_member());
  EXPECT_EQ(2, other_property.AsType<DerivedDictionary>().long_member());

  // Union can still accept an arbitrary object as well
  EXPECT_CALL(test_mock(), set_union_dicts_objects_property(_))
      .WillOnce(SaveArg<0>(&other_property));
  EXPECT_TRUE(EvaluateScript(
      "test.unionDictsObjectsProperty = new ArbitraryInterface();", NULL));
  ASSERT_TRUE(other_property.IsType<scoped_refptr<ArbitraryInterface> >());
  EXPECT_TRUE(other_property.AsType<scoped_refptr<ArbitraryInterface> >());
}

TEST_F(UnionTypesBindingsTest, ConvertDictToJS) {
  InSequence dummy;

  // Return a string
  std::string result;
  EXPECT_CALL(test_mock(), union_with_dictionary_property())
      .WillOnce(Return(
          UnionTypesInterface::UnionDictPropertyType(std::string("foo"))));
  EXPECT_TRUE(EvaluateScript(
      "typeof test.unionWithDictionaryProperty == \"string\";", &result));
  EXPECT_EQ("true", result);

  // Return a dictionary with default member set
  EXPECT_CALL(test_mock(), union_with_dictionary_property())
      .WillOnce(
          Return(UnionTypesInterface::UnionDictPropertyType(TestDictionary())));
  EXPECT_TRUE(EvaluateScript(
      "test.unionWithDictionaryProperty['memberWithDefault']", &result));
  EXPECT_EQ("5", result);

  // Return an arbitrary object
  EXPECT_CALL(test_mock(), union_dicts_objects_property())
      .WillOnce(Return(UnionTypesInterface::UnionObjectsPropertyType(
          base::WrapRefCounted(new ArbitraryInterface()))));
  EXPECT_TRUE(EvaluateScript(
      "Object.getPrototypeOf(test.unionDictsObjectsProperty) === "
      "ArbitraryInterface.prototype;",
      &result));
  EXPECT_EQ("true", result);

  // Return a derived dictionary with default boolean member set
  EXPECT_CALL(test_mock(), union_dicts_objects_property())
      .WillOnce(Return(
          UnionTypesInterface::UnionObjectsPropertyType(DerivedDictionary())));
  EXPECT_TRUE(EvaluateScript(
      "test.unionDictsObjectsProperty['additionalMember'] == false", &result));
  EXPECT_EQ("true", result);
}

TEST_F(UnionTypesBindingsTest, ConvertDictFromJSInvalid) {
  InSequence dummy;

  std::string result;
  EXPECT_FALSE(EvaluateScript("test.unionDictsObjectsProperty = 42;", &result));
  EXPECT_THAT(result, ContainsRegex("TypeError:"));

  EXPECT_FALSE(
      EvaluateScript("test.unionDictsObjectsProperty = 'foo';", &result));
  EXPECT_THAT(result, ContainsRegex("TypeError:"));

  EXPECT_FALSE(
      EvaluateScript("test.unionDictsObjectsProperty = true;", &result));
  EXPECT_THAT(result, ContainsRegex("TypeError:"));
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

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
#include "cobalt/bindings/testing/nullable_types_test_interface.h"
#include "cobalt/bindings/testing/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {
typedef InterfaceBindingsTest<NullableTypesTestInterface>
    NullableTypesBindingsTest;
}  // namespace

// EXPECT_CALL needs a compare operator to check the parameter.
bool operator==(const TestDictionary& a, const TestDictionary& b) {
  // clang-format off
  return
      (!a.has_boolean_member()
           ? !b.has_boolean_member()
           : a.boolean_member() == b.boolean_member()) &&
      (!a.has_short_clamp_member()
           ? !b.has_short_clamp_member()
           : a.short_clamp_member() == b.short_clamp_member()) &&
      (!a.has_long_member()
           ? !b.has_long_member()
           : a.long_member() == b.long_member()) &&
      (!a.has_double_member()
           ? !b.has_double_member()
           : a.double_member() == b.double_member()) &&
      (!a.has_string_member()
           ? !b.has_string_member()
           : a.string_member() == b.string_member()) &&
      (!a.has_interface_member()
           ? !b.has_interface_member()
           : a.interface_member() == b.interface_member()) &&
      (!a.has_member_with_default()
           ? !b.has_member_with_default()
           : a.member_with_default() == b.member_with_default()) &&
      (!a.has_non_default_member()
           ? !b.has_non_default_member()
           : a.non_default_member() == b.non_default_member()) &&
      (!a.has_any_member_with_default()
           ? !b.has_any_member_with_default()
           : a.any_member_with_default() == b.any_member_with_default()) &&
      (!a.has_any_member()
           ? !b.has_any_member()
           : a.any_member() == b.any_member());
  // clang-format on
}

TEST_F(NullableTypesBindingsTest, GetNullProperty) {
  InSequence in_sequence_dummy;
  std::string result;

  EXPECT_CALL(test_mock(), nullable_boolean_property())
      .WillOnce(Return(base::nullopt));
  EXPECT_TRUE(
      EvaluateScript("test.nullableBooleanProperty === null;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), nullable_numeric_property())
      .WillOnce(Return(base::nullopt));
  EXPECT_TRUE(
      EvaluateScript("test.nullableNumericProperty === null;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), nullable_string_property())
      .WillOnce(Return(base::nullopt));
  EXPECT_TRUE(EvaluateScript("test.nullableStringProperty === null;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), nullable_dictionary_property())
      .WillOnce(Return(base::nullopt));
  EXPECT_TRUE(
      EvaluateScript("test.nullableDictionaryProperty === null;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), nullable_object_property())
      .WillOnce(Return(scoped_refptr<ArbitraryInterface>()));
  EXPECT_TRUE(EvaluateScript("test.nullableObjectProperty === null;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(NullableTypesBindingsTest, GetNonNullProperty) {
  InSequence in_sequence_dummy;
  std::string result;

  EXPECT_CALL(test_mock(), nullable_boolean_property())
      .WillOnce(Return(base::Optional<bool>(true)));
  EXPECT_TRUE(
      EvaluateScript("test.nullableBooleanProperty === true;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), nullable_numeric_property())
      .WillOnce(Return(base::Optional<int32_t>(5)));
  EXPECT_TRUE(EvaluateScript("test.nullableNumericProperty === 5;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), nullable_string_property())
      .WillOnce(Return(base::Optional<std::string>("foo")));
  EXPECT_TRUE(
      EvaluateScript("test.nullableStringProperty === \"foo\";", &result));
  EXPECT_STREQ("true", result.c_str());

  TestDictionary test_dictionary;
  test_dictionary.set_string_member("foobar");
  EXPECT_CALL(test_mock(), nullable_dictionary_property())
      .WillOnce(Return(base::Optional<TestDictionary>(test_dictionary)));
  EXPECT_TRUE(
      EvaluateScript("test.nullableDictionaryProperty.stringMember;", &result));
  EXPECT_STREQ("foobar", result.c_str());

  EXPECT_CALL(test_mock(), nullable_object_property())
      .WillOnce(Return(base::WrapRefCounted(new ArbitraryInterface())));
  EXPECT_TRUE(
      EvaluateScript("test.nullableObjectProperty.__proto__;", &result));
  EXPECT_TRUE(IsAcceptablePrototypeString("ArbitraryInterface", result))
      << result;
}

TEST_F(NullableTypesBindingsTest, ReturnNullFromOperation) {
  InSequence in_sequence_dummy;
  std::string result;

  EXPECT_CALL(test_mock(), NullableBooleanOperation())
      .WillOnce(Return(base::nullopt));
  EXPECT_TRUE(
      EvaluateScript("test.nullableBooleanOperation() === null;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), NullableNumericOperation())
      .WillOnce(Return(base::nullopt));
  EXPECT_TRUE(
      EvaluateScript("test.nullableNumericOperation() === null;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), NullableStringOperation())
      .WillOnce(Return(base::nullopt));
  EXPECT_TRUE(
      EvaluateScript("test.nullableStringOperation() === null;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), NullableDictionaryOperation())
      .WillOnce(Return(base::nullopt));
  EXPECT_TRUE(
      EvaluateScript("test.nullableDictionaryOperation() === null;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), NullableObjectOperation())
      .WillOnce(Return(scoped_refptr<ArbitraryInterface>()));
  EXPECT_TRUE(
      EvaluateScript("test.nullableObjectOperation() === null;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(NullableTypesBindingsTest, ReturnNonNullFromOperation) {
  InSequence in_sequence_dummy;
  std::string result;

  EXPECT_CALL(test_mock(), NullableBooleanOperation())
      .WillOnce(Return(base::Optional<bool>(true)));
  EXPECT_TRUE(
      EvaluateScript("test.nullableBooleanOperation() === true;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), NullableNumericOperation())
      .WillOnce(Return(base::Optional<int32_t>(5)));
  EXPECT_TRUE(
      EvaluateScript("test.nullableNumericOperation() === 5;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), NullableStringOperation())
      .WillOnce(Return(base::Optional<std::string>("foo")));
  EXPECT_TRUE(
      EvaluateScript("test.nullableStringOperation() === \"foo\";", &result));
  EXPECT_STREQ("true", result.c_str());

  TestDictionary test_dictionary;
  test_dictionary.set_string_member("foobar");
  EXPECT_CALL(test_mock(), NullableDictionaryOperation())
      .WillOnce(Return(base::Optional<TestDictionary>(test_dictionary)));
  EXPECT_TRUE(EvaluateScript("test.nullableDictionaryOperation().stringMember;",
                             &result));
  EXPECT_STREQ("foobar", result.c_str());

  EXPECT_CALL(test_mock(), NullableObjectOperation())
      .WillOnce(Return(base::WrapRefCounted(new ArbitraryInterface())));
  EXPECT_TRUE(
      EvaluateScript("test.nullableObjectOperation().__proto__;", &result));
  EXPECT_TRUE(IsAcceptablePrototypeString("ArbitraryInterface", result))
      << result;
}

TEST_F(NullableTypesBindingsTest, SetNullProperty) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(),
              set_nullable_boolean_property(base::Optional<bool>()));
  EXPECT_TRUE(EvaluateScript("test.nullableBooleanProperty = null;", NULL));

  EXPECT_CALL(test_mock(),
              set_nullable_numeric_property(base::Optional<int32_t>()));
  EXPECT_TRUE(EvaluateScript("test.nullableNumericProperty = null;", NULL));

  EXPECT_CALL(test_mock(),
              set_nullable_string_property(base::Optional<std::string>()));
  EXPECT_TRUE(EvaluateScript("test.nullableStringProperty = null;", NULL));

  EXPECT_CALL(test_mock(), set_nullable_dictionary_property(
                               base::Optional<TestDictionary>()));
  EXPECT_TRUE(EvaluateScript("test.nullableDictionaryProperty = null;", NULL));

  EXPECT_CALL(test_mock(), set_nullable_object_property(
                               scoped_refptr<ArbitraryInterface>()));
  EXPECT_TRUE(EvaluateScript("test.nullableObjectProperty = null;", NULL));
}

TEST_F(NullableTypesBindingsTest, SetNonNullProperty) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(),
              set_nullable_boolean_property(base::Optional<bool>(true)));
  EXPECT_TRUE(EvaluateScript("test.nullableBooleanProperty = true;", NULL));

  EXPECT_CALL(test_mock(),
              set_nullable_numeric_property(base::Optional<int32_t>(5)));
  EXPECT_TRUE(EvaluateScript("test.nullableNumericProperty = 5;", NULL));

  EXPECT_CALL(test_mock(),
              set_nullable_string_property(base::Optional<std::string>("foo")));
  EXPECT_TRUE(EvaluateScript("test.nullableStringProperty = \"foo\";", NULL));

  TestDictionary test_dictionary;
  test_dictionary.set_string_member("foobar");
  EXPECT_CALL(test_mock(),
              set_nullable_dictionary_property(
                  base::Optional<TestDictionary>(test_dictionary)));
  EXPECT_TRUE(EvaluateScript(
      "test.nullableDictionaryProperty = {stringMember: \"foobar\"};", NULL));

  scoped_refptr<ArbitraryInterface> mock_interface = new ArbitraryInterface();
  EXPECT_CALL(test_mock(), NullableObjectOperation())
      .WillOnce(Return(mock_interface));
  EXPECT_CALL(test_mock(), set_nullable_object_property(mock_interface));
  EXPECT_TRUE(EvaluateScript(
      "test.nullableObjectProperty = test.nullableObjectOperation();", NULL));
}

TEST_F(NullableTypesBindingsTest, PassNullArgument) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(), NullableBooleanArgument(base::Optional<bool>()));
  EXPECT_TRUE(EvaluateScript("test.nullableBooleanArgument(null);", NULL));

  EXPECT_CALL(test_mock(), NullableNumericArgument(base::Optional<int32_t>()));
  EXPECT_TRUE(EvaluateScript("test.nullableNumericArgument(null);", NULL));

  EXPECT_CALL(test_mock(),
              NullableStringArgument(base::Optional<std::string>()));
  EXPECT_TRUE(EvaluateScript("test.nullableStringArgument(null);", NULL));

  EXPECT_CALL(test_mock(),
              NullableDictionaryArgument(base::Optional<TestDictionary>()));
  EXPECT_TRUE(EvaluateScript("test.nullableDictionaryArgument(null);", NULL));

  EXPECT_CALL(test_mock(),
              NullableObjectArgument(scoped_refptr<ArbitraryInterface>()));
  EXPECT_TRUE(EvaluateScript("test.nullableObjectArgument(null);", NULL));
}

TEST_F(NullableTypesBindingsTest, PassNonNullArgument) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(), NullableBooleanArgument(base::Optional<bool>(true)));
  EXPECT_TRUE(EvaluateScript("test.nullableBooleanArgument(true);", NULL));

  EXPECT_CALL(test_mock(), NullableNumericArgument(base::Optional<int32_t>(5)));
  EXPECT_TRUE(EvaluateScript("test.nullableNumericArgument(5);", NULL));

  EXPECT_CALL(test_mock(),
              NullableStringArgument(base::Optional<std::string>("foo")));
  EXPECT_TRUE(EvaluateScript("test.nullableStringArgument(\"foo\");", NULL));

  TestDictionary test_dictionary;
  test_dictionary.set_string_member("foobar");
  EXPECT_CALL(test_mock(),
              NullableDictionaryArgument(
                  base::Optional<TestDictionary>(test_dictionary)));
  EXPECT_TRUE(EvaluateScript(
      "test.nullableDictionaryArgument({stringMember: \"foobar\"});", NULL));

  scoped_refptr<ArbitraryInterface> mock_interface = new ArbitraryInterface();
  EXPECT_CALL(test_mock(), nullable_object_property())
      .WillOnce(Return(mock_interface));
  EXPECT_CALL(test_mock(), NullableObjectArgument(mock_interface));
  EXPECT_TRUE(EvaluateScript(
      "test.nullableObjectArgument(test.nullableObjectProperty);", NULL));
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

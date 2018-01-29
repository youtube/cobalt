// Copyright 2015 Google Inc. All Rights Reserved.
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

  EXPECT_CALL(test_mock(), nullable_object_property())
      .WillOnce(Return(scoped_refptr<ArbitraryInterface>()));
  EXPECT_TRUE(EvaluateScript("test.nullableObjectProperty === null;", &result));
  EXPECT_STREQ("true", result.c_str());
}

TEST_F(NullableTypesBindingsTest, GetNonNullProperty) {
  InSequence in_sequence_dummy;
  std::string result;

  EXPECT_CALL(test_mock(), nullable_boolean_property())
      .WillOnce(Return(base::optional<bool>(true)));
  EXPECT_TRUE(
      EvaluateScript("test.nullableBooleanProperty === true;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), nullable_numeric_property())
      .WillOnce(Return(base::optional<int32_t>(5)));
  EXPECT_TRUE(EvaluateScript("test.nullableNumericProperty === 5;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), nullable_string_property())
      .WillOnce(Return(base::optional<std::string>("foo")));
  EXPECT_TRUE(
      EvaluateScript("test.nullableStringProperty === \"foo\";", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), nullable_object_property())
      .WillOnce(Return(make_scoped_refptr(new ArbitraryInterface())));
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
      .WillOnce(Return(base::optional<bool>(true)));
  EXPECT_TRUE(
      EvaluateScript("test.nullableBooleanOperation() === true;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), NullableNumericOperation())
      .WillOnce(Return(base::optional<int32_t>(5)));
  EXPECT_TRUE(
      EvaluateScript("test.nullableNumericOperation() === 5;", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), NullableStringOperation())
      .WillOnce(Return(base::optional<std::string>("foo")));
  EXPECT_TRUE(
      EvaluateScript("test.nullableStringOperation() === \"foo\";", &result));
  EXPECT_STREQ("true", result.c_str());

  EXPECT_CALL(test_mock(), NullableObjectOperation())
      .WillOnce(Return(make_scoped_refptr(new ArbitraryInterface())));
  EXPECT_TRUE(
      EvaluateScript("test.nullableObjectOperation().__proto__;", &result));
  EXPECT_TRUE(IsAcceptablePrototypeString("ArbitraryInterface", result))
      << result;
}

TEST_F(NullableTypesBindingsTest, SetNullProperty) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(),
              set_nullable_boolean_property(base::optional<bool>()));
  EXPECT_TRUE(EvaluateScript("test.nullableBooleanProperty = null;", NULL));

  EXPECT_CALL(test_mock(),
              set_nullable_numeric_property(base::optional<int32_t>()));
  EXPECT_TRUE(EvaluateScript("test.nullableNumericProperty = null;", NULL));

  EXPECT_CALL(test_mock(),
              set_nullable_string_property(base::optional<std::string>()));
  EXPECT_TRUE(EvaluateScript("test.nullableStringProperty = null;", NULL));

  EXPECT_CALL(test_mock(), set_nullable_object_property(
                               scoped_refptr<ArbitraryInterface>()));
  EXPECT_TRUE(EvaluateScript("test.nullableObjectProperty = null;", NULL));
}

TEST_F(NullableTypesBindingsTest, SetNonNullProperty) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(),
              set_nullable_boolean_property(base::optional<bool>(true)));
  EXPECT_TRUE(EvaluateScript("test.nullableBooleanProperty = true;", NULL));

  EXPECT_CALL(test_mock(),
              set_nullable_numeric_property(base::optional<int32_t>(5)));
  EXPECT_TRUE(EvaluateScript("test.nullableNumericProperty = 5;", NULL));

  EXPECT_CALL(test_mock(),
              set_nullable_string_property(base::optional<std::string>("foo")));
  EXPECT_TRUE(EvaluateScript("test.nullableStringProperty = \"foo\";", NULL));

  scoped_refptr<ArbitraryInterface> mock_interface = new ArbitraryInterface();
  EXPECT_CALL(test_mock(), NullableObjectOperation())
      .WillOnce(Return(mock_interface));
  EXPECT_CALL(test_mock(), set_nullable_object_property(mock_interface));
  EXPECT_TRUE(EvaluateScript(
      "test.nullableObjectProperty = test.nullableObjectOperation();", NULL));
}

TEST_F(NullableTypesBindingsTest, PassNullArgument) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(), NullableBooleanArgument(base::optional<bool>()));
  EXPECT_TRUE(EvaluateScript("test.nullableBooleanArgument(null);", NULL));

  EXPECT_CALL(test_mock(), NullableNumericArgument(base::optional<int32_t>()));
  EXPECT_TRUE(EvaluateScript("test.nullableNumericArgument(null);", NULL));

  EXPECT_CALL(test_mock(),
              NullableStringArgument(base::optional<std::string>()));
  EXPECT_TRUE(EvaluateScript("test.nullableStringArgument(null);", NULL));

  EXPECT_CALL(test_mock(),
              NullableObjectArgument(scoped_refptr<ArbitraryInterface>()));
  EXPECT_TRUE(EvaluateScript("test.nullableObjectArgument(null);", NULL));
}

TEST_F(NullableTypesBindingsTest, PassNonNullArgument) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(test_mock(), NullableBooleanArgument(base::optional<bool>(true)));
  EXPECT_TRUE(EvaluateScript("test.nullableBooleanArgument(true);", NULL));

  EXPECT_CALL(test_mock(), NullableNumericArgument(base::optional<int32_t>(5)));
  EXPECT_TRUE(EvaluateScript("test.nullableNumericArgument(5);", NULL));

  EXPECT_CALL(test_mock(),
              NullableStringArgument(base::optional<std::string>("foo")));
  EXPECT_TRUE(EvaluateScript("test.nullableStringArgument(\"foo\");", NULL));

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

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

#include <cmath>

#include "base/stringprintf.h"
#include "cobalt/bindings/testing/bindings_test_base.h"
#include "cobalt/bindings/testing/numeric_types_test_interface.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;
using ::testing::InSequence;
using ::testing::ResultOf;
using ::testing::Return;

namespace cobalt {
namespace bindings {
namespace testing {

namespace {

template <typename T>
class NumericTypeBindingsTest
    : public InterfaceBindingsTest<T, NumericTypesTestInterface> {
 public:
};

template <typename T>
class IntegerTypeBindingsTest : public NumericTypeBindingsTest<T> {};

template <typename T>
class FloatingPointTypeBindingsTest : public NumericTypeBindingsTest<T> {};

typedef ::testing::Types<ByteTypeTest, OctetTypeTest, ShortTypeTest,
                         UnsignedShortTypeTest, LongTypeTest,
                         UnsignedLongTypeTest, DoubleTypeTest> NumericTypes;
typedef ::testing::Types<ByteTypeTest, OctetTypeTest, ShortTypeTest,
                         UnsignedShortTypeTest, LongTypeTest,
                         UnsignedLongTypeTest> IntegerTypes;
typedef ::testing::Types<DoubleTypeTest, UnrestrictedDoubleTypeTest>
    FloatingPointTypes;
TYPED_TEST_CASE(NumericTypeBindingsTest, NumericTypes);
TYPED_TEST_CASE(IntegerTypeBindingsTest, IntegerTypes);
TYPED_TEST_CASE(FloatingPointTypeBindingsTest, FloatingPointTypes);

template <typename T>
bool IsNan(T number) {
#if defined(_MSC_VER)
#pragma warning(push)
// On Windows isnan() returns an int.
// warning C4800: 'int' : forcing value to bool 'true' or 'false'
#pragma warning(disable:4800)
#endif
  return isnan(number);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
}

}  // namespace

TYPED_TEST(NumericTypeBindingsTest, PropertyIsNumber) {
  EXPECT_CALL(this->test_mock(), mock_get_property());
  std::string result;
  std::string script =
      StringPrintf("typeof test.%sProperty;", TypeParam::type_string());
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ("number", result.c_str());
}

TYPED_TEST(NumericTypeBindingsTest, ReturnValueIsNumber) {
  EXPECT_CALL(this->test_mock(), MockReturnValueOperation());
  std::string result;
  std::string script = StringPrintf("typeof test.%sReturnOperation();",
                                    TypeParam::type_string());
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ("number", result.c_str());
}

TYPED_TEST(IntegerTypeBindingsTest, PropertyValueRange) {
  InSequence in_sequence_dummy;

  std::string result;
  std::string script =
      StringPrintf("test.%sProperty;", TypeParam::type_string());

  EXPECT_CALL(this->test_mock(), mock_get_property()).WillOnce(Return(0));
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ("0", result.c_str());

  EXPECT_CALL(this->test_mock(), mock_get_property())
      .WillOnce(Return(TypeParam::min_value()));
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ(TypeParam::min_value_string(), result.c_str());

  EXPECT_CALL(this->test_mock(), mock_get_property())
      .WillOnce(Return(TypeParam::max_value()));
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ(TypeParam::max_value_string(), result.c_str());
}

TYPED_TEST(IntegerTypeBindingsTest, ReturnValueRange) {
  InSequence in_sequence_dummy;

  std::string result;
  std::string script =
      StringPrintf("test.%sReturnOperation();", TypeParam::type_string());
  EXPECT_CALL(this->test_mock(), MockReturnValueOperation())
      .WillOnce(Return(0));
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ("0", result.c_str());

  EXPECT_CALL(this->test_mock(), MockReturnValueOperation())
      .WillOnce(Return(TypeParam::min_value()));
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ(TypeParam::min_value_string(), result.c_str());

  EXPECT_CALL(this->test_mock(), MockReturnValueOperation())
      .WillOnce(Return(TypeParam::max_value()));
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ(TypeParam::max_value_string(), result.c_str());
}

TYPED_TEST(IntegerTypeBindingsTest, SetPropertyRange) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(this->test_mock(), mock_set_property(0));
  EXPECT_TRUE(this->EvaluateScript(
      StringPrintf("test.%sProperty = 0;", TypeParam::type_string()), NULL));

  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::min_value()));
  EXPECT_TRUE(this->EvaluateScript(
      StringPrintf("test.%sProperty = %s;", TypeParam::type_string(),
                   TypeParam::min_value_string()),
      NULL));

  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::max_value()));
  EXPECT_TRUE(this->EvaluateScript(
      StringPrintf("test.%sProperty = %s;", TypeParam::type_string(),
                   TypeParam::max_value_string()),
      NULL));
}

TYPED_TEST(IntegerTypeBindingsTest, ArgumentOperationRange) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(this->test_mock(), MockArgumentOperation(0));
  EXPECT_TRUE(this->EvaluateScript(
      StringPrintf("test.%sArgumentOperation(0);", TypeParam::type_string()),
      NULL));

  EXPECT_CALL(this->test_mock(), MockArgumentOperation(TypeParam::max_value()));
  EXPECT_TRUE(this->EvaluateScript(
      StringPrintf("test.%sArgumentOperation(%s);", TypeParam::type_string(),
                   TypeParam::max_value_string()),
      NULL));

  EXPECT_CALL(this->test_mock(), MockArgumentOperation(TypeParam::min_value()));
  EXPECT_TRUE(this->EvaluateScript(
      StringPrintf("test.%sArgumentOperation(%s);", TypeParam::type_string(),
                   TypeParam::min_value_string()),
      NULL));
}

// In the absence of extended IDL attributes to check or enforce the range,
// out-of-range values are not clamped.
// https://www.w3.org/TR/WebIDL/#es-byte
// For the signed types (8 bit integer in this example):
//     5. x := sign(x)*floor(abs(x))
//     6. x := x modulo 2^8
//     7. return (x >= 2^7) ? x - 2^8 : x
// For unsigned types, skip step 7.
TYPED_TEST(IntegerTypeBindingsTest, OutOfRangeBehaviour) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::min_value()));
  EXPECT_TRUE(this->EvaluateScript(
      StringPrintf("test.%sProperty = (%s+1);", TypeParam::type_string(),
                   TypeParam::max_value_string()),
      NULL));
  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::min_value() + 1));
  EXPECT_TRUE(this->EvaluateScript(
      StringPrintf("test.%sProperty = (%s+2);", TypeParam::type_string(),
                   TypeParam::max_value_string()),
      NULL));

  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::max_value()));
  EXPECT_TRUE(this->EvaluateScript(
      StringPrintf("test.%sProperty = (%s-1);", TypeParam::type_string(),
                   TypeParam::min_value_string()),
      NULL));
  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::max_value() - 1));
  EXPECT_TRUE(this->EvaluateScript(
      StringPrintf("test.%sProperty = (%s-2);", TypeParam::type_string(),
                   TypeParam::min_value_string()),
      NULL));
}

TYPED_TEST(FloatingPointTypeBindingsTest, NonFiniteValues) {
  InSequence in_sequence_dummy;
  if (TypeParam::is_restricted()) {
    EXPECT_FALSE(this->EvaluateScript(
        StringPrintf("test.%sProperty = Infinity;", TypeParam::type_string()),
        NULL));
    EXPECT_FALSE(this->EvaluateScript(
        StringPrintf("test.%sProperty = -Infinity;", TypeParam::type_string()),
        NULL));
    EXPECT_FALSE(this->EvaluateScript(
        StringPrintf("test.%sProperty = NaN;", TypeParam::type_string()),
        NULL));
  } else {
    EXPECT_CALL(this->test_mock(),
                mock_set_property(TypeParam::positive_infinity()));
    EXPECT_TRUE(this->EvaluateScript(
        StringPrintf("test.%sProperty = Infinity;", TypeParam::type_string()),
        NULL));

    EXPECT_CALL(this->test_mock(),
                mock_set_property(TypeParam::negative_infinity()));
    EXPECT_TRUE(this->EvaluateScript(
        StringPrintf("test.%sProperty = -Infinity;", TypeParam::type_string()),
        NULL));

    EXPECT_CALL(this->test_mock(),
                mock_set_property(
                    ResultOf(IsNan<typename TypeParam::BaseType>, Eq(true))));
    EXPECT_TRUE(this->EvaluateScript(
        StringPrintf("test.%sProperty = NaN;", TypeParam::type_string()),
        NULL));
  }
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

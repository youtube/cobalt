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

#include <cmath>

#include "base/strings/stringprintf.h"
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

#if defined(ENGINE_SUPPORTS_INT64)
template <typename T>
class LargeIntegerTypeBindingsTest : public NumericTypeBindingsTest<T> {};

typedef ::testing::Types<ByteTypeTest, OctetTypeTest, ShortTypeTest,
                         UnsignedShortTypeTest, LongTypeTest,
                         UnsignedLongTypeTest, LongLongTypeTest,
                         UnsignedLongLongTypeTest, DoubleTypeTest>
    NumericTypes;

typedef ::testing::Types<LongLongTypeTest, UnsignedLongLongTypeTest>
    LargeIntegerTypes;

TYPED_TEST_CASE(LargeIntegerTypeBindingsTest, LargeIntegerTypes);
#else
typedef ::testing::Types<ByteTypeTest, OctetTypeTest, ShortTypeTest,
                         UnsignedShortTypeTest, LongTypeTest,
                         UnsignedLongTypeTest, DoubleTypeTest>
    NumericTypes;
#endif  // ENGINE_SUPPORTS_INT64
// Not including long longs in IntegerTypes, due to different casting
// behaviours.
typedef ::testing::Types<ByteTypeTest, OctetTypeTest, ShortTypeTest,
                         UnsignedShortTypeTest, LongTypeTest,
                         UnsignedLongTypeTest>
    IntegerTypes;

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
#pragma warning(disable : 4800)
#endif
  return std::isnan(number);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
}

}  // namespace

TYPED_TEST(NumericTypeBindingsTest, PropertyIsNumber) {
  EXPECT_CALL(this->test_mock(), mock_get_property());
  std::string result;
  std::string script =
      base::StringPrintf("typeof test.%sProperty;", TypeParam::type_string());
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ("number", result.c_str());
}

TYPED_TEST(NumericTypeBindingsTest, ReturnValueIsNumber) {
  EXPECT_CALL(this->test_mock(), MockReturnValueOperation());
  std::string result;
  std::string script = base::StringPrintf("typeof test.%sReturnOperation();",
                                          TypeParam::type_string());
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ("number", result.c_str());
}

TYPED_TEST(IntegerTypeBindingsTest, PropertyValueRange) {
  InSequence in_sequence_dummy;

  std::string result;
  std::string script =
      base::StringPrintf("test.%sProperty;", TypeParam::type_string());

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
      base::StringPrintf("test.%sReturnOperation();", TypeParam::type_string());
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
      base::StringPrintf("test.%sProperty = 0;", TypeParam::type_string()),
      NULL));
  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::min_value()));
  EXPECT_TRUE(this->EvaluateScript(
      base::StringPrintf("test.%sProperty = %s;", TypeParam::type_string(),
                         TypeParam::min_value_string()),
      NULL));

  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::max_value()));
  EXPECT_TRUE(this->EvaluateScript(
      base::StringPrintf("test.%sProperty = %s;", TypeParam::type_string(),
                         TypeParam::max_value_string()),
      NULL));
}

TYPED_TEST(IntegerTypeBindingsTest, ArgumentOperationRange) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(this->test_mock(), MockArgumentOperation(0));
  EXPECT_TRUE(
      this->EvaluateScript(base::StringPrintf("test.%sArgumentOperation(0);",
                                              TypeParam::type_string()),
                           NULL));

  EXPECT_CALL(this->test_mock(), MockArgumentOperation(TypeParam::max_value()));
  EXPECT_TRUE(
      this->EvaluateScript(base::StringPrintf("test.%sArgumentOperation(%s);",
                                              TypeParam::type_string(),
                                              TypeParam::max_value_string()),
                           NULL));

  EXPECT_CALL(this->test_mock(), MockArgumentOperation(TypeParam::min_value()));
  EXPECT_TRUE(
      this->EvaluateScript(base::StringPrintf("test.%sArgumentOperation(%s);",
                                              TypeParam::type_string(),
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
      base::StringPrintf("test.%sProperty = (%s+1);", TypeParam::type_string(),
                         TypeParam::max_value_string()),
      NULL));
  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::min_value() + 1));
  EXPECT_TRUE(this->EvaluateScript(
      base::StringPrintf("test.%sProperty = (%s+2);", TypeParam::type_string(),
                         TypeParam::max_value_string()),
      NULL));

  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::max_value()));
  EXPECT_TRUE(this->EvaluateScript(
      base::StringPrintf("test.%sProperty = (%s-1);", TypeParam::type_string(),
                         TypeParam::min_value_string()),
      NULL));
  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::max_value() - 1));
  EXPECT_TRUE(this->EvaluateScript(
      base::StringPrintf("test.%sProperty = (%s-2);", TypeParam::type_string(),
                         TypeParam::min_value_string()),
      NULL));
}

// With the extended IDL attribute Clamp, out-of-range values are clamped.
// https://www.w3.org/TR/WebIDL/#es-byte
// For the signed types (8 bit integer in this example):
//     If x is not NaN and the conversion to an IDL value is being
//     performed due to any of the following: ...[Clamp] extended attribute...
//     then:
//     1. Set x to min(max(x, −2^7), 2^7 − 1).
//     2. Round x to the nearest integer, choosing the even integer if
//     it lies halfway between two, and choosing +0 rather than −0.
//     3. Return the IDL byte value that represents the same numeric value as x.
TYPED_TEST(IntegerTypeBindingsTest, ClampedOutOfRangeBehaviour) {
  InSequence in_sequence_dummy;

  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::max_value()));
  EXPECT_TRUE(
      this->EvaluateScript(base::StringPrintf("test.%sClampProperty = (%s+1);",
                                              TypeParam::type_string(),
                                              TypeParam::max_value_string()),
                           NULL));

  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::max_value()));
  EXPECT_TRUE(
      this->EvaluateScript(base::StringPrintf("test.%sClampProperty = (%s+2);",
                                              TypeParam::type_string(),
                                              TypeParam::max_value_string()),
                           NULL));

  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::min_value()));
  EXPECT_TRUE(
      this->EvaluateScript(base::StringPrintf("test.%sClampProperty = (%s-1);",
                                              TypeParam::type_string(),
                                              TypeParam::min_value_string()),
                           NULL));

  EXPECT_CALL(this->test_mock(), mock_set_property(TypeParam::min_value()));
  EXPECT_TRUE(
      this->EvaluateScript(base::StringPrintf("test.%sClampProperty = (%s-2);",
                                              TypeParam::type_string(),
                                              TypeParam::min_value_string()),
                           NULL));
}

#if defined(ENGINE_SUPPORTS_INT64)
TYPED_TEST(LargeIntegerTypeBindingsTest, PropertyValueRange) {
  InSequence in_sequence_dummy;

  std::string result;
  std::string script =
      base::StringPrintf("test.%sProperty;", TypeParam::type_string());

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

// These tests require converting LargeIntegers (e.g. long long) to
// JSValues using the IDL spec:
// https://www.w3.org/TR/2012/CR-WebIDL-20120419/#es-long-long
// This preserves exactly the range (-(2^53 - 1), 2^53 -1) and
// approximately outside that range (see the spec for details).
TYPED_TEST(LargeIntegerTypeBindingsTest, ReturnValueRange) {
  InSequence in_sequence_dummy;

  // Exactly preserve 0.
  std::string result;
  std::string script =
      base::StringPrintf("test.%sReturnOperation();", TypeParam::type_string());
  EXPECT_CALL(this->test_mock(), MockReturnValueOperation())
      .WillOnce(Return(0));
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ("0", result.c_str());

  // Approximately preserve int64_t/uint64_t min.
  EXPECT_CALL(this->test_mock(), MockReturnValueOperation())
      .WillOnce(Return(TypeParam::min_value()));
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ(TypeParam::min_value_string(), result.c_str());

  // Approximately preserve int64_t/uint64_t max.
  EXPECT_CALL(this->test_mock(), MockReturnValueOperation())
      .WillOnce(Return(TypeParam::max_value()));
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ(TypeParam::max_value_string(), result.c_str());

  // Exactly preserve 2^53 - 1.
  const uint64_t kRangeBound = (1ll << 53) - 1;
  std::string expected_result = base::StringPrintf("%" PRIu64 "", kRangeBound);
  EXPECT_CALL(this->test_mock(), MockReturnValueOperation())
      .WillOnce(Return(kRangeBound));
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ(expected_result.c_str(), result.c_str());

  // Signed : exactly preserve -(2^53 - 1).
  if (TypeParam::min_value() < 0) {
    expected_result = base::StringPrintf("-%" PRIu64 "", kRangeBound);
    EXPECT_CALL(this->test_mock(), MockReturnValueOperation())
        .WillOnce(Return(-static_cast<int64_t>(kRangeBound)));
    EXPECT_TRUE(this->EvaluateScript(script, &result));
    EXPECT_STREQ(expected_result.c_str(), result.c_str());
  }

  // Exactly preserve 9223372036854775000 (between 2^53 and int64_t max).
  expected_result = "9223372036854775000";
  EXPECT_CALL(this->test_mock(), MockReturnValueOperation())
      .WillOnce(Return(9223372036854775000ll));
  EXPECT_TRUE(this->EvaluateScript(script, &result));
  EXPECT_STREQ(expected_result.c_str(), result.c_str());

  // Unsigned : exactly preserve 18446744073709550000 (between 2^53
  // and uint64_t max).
  if (TypeParam::min_value() >= 0) {
    expected_result = "18446744073709550000";
    EXPECT_CALL(this->test_mock(), MockReturnValueOperation())
        .WillOnce(Return(18446744073709550000ull));
    EXPECT_TRUE(this->EvaluateScript(script, &result));
    EXPECT_STREQ(expected_result.c_str(), result.c_str());
  }
}

// These tests require converting JSValues to LargeIntegers (e.g. long long)
// using the IDL spec:
// https://www.w3.org/TR/2012/CR-WebIDL-20120419/#es-long-long
// This preserves exactly the range (-(2^53 - 1), 2^53 -1),
// and for all other values does the following:
// For input value V.
// x = ToNumber(V)
// If x is Nan, +inf, -inf return 0
// ...Handle extended_attribute special cases...
// 5. x = sign(x) * floor(abs(x))
// 6. x = x mod 2^64
// 7. If x >= 2^63, x = x - 2^64 (for signed only)
// 8. Return the IDL long long value that represents the same numeric
//    value as x.
TYPED_TEST(LargeIntegerTypeBindingsTest, SetPropertyRange) {
  InSequence in_sequence_dummy;

  // Exactly preserve 0.
  EXPECT_CALL(this->test_mock(), mock_set_property(0));
  EXPECT_TRUE(this->EvaluateScript(
      base::StringPrintf("test.%sProperty = 0;", TypeParam::type_string()),
      NULL));

  // Exactly preserve 2^53 - 1.
  EXPECT_CALL(this->test_mock(), mock_set_property((1ll << 53) - 1));
  EXPECT_TRUE(this->EvaluateScript(
      base::StringPrintf("test.%sProperty = 9007199254740991;",
                         TypeParam::type_string()),
      NULL));

  // Signed : exactly preserve -(2^53 - 1).
  if (TypeParam::min_value() < 0) {
    EXPECT_CALL(this->test_mock(), mock_set_property(-((1ll << 53) - 1)));
    EXPECT_TRUE(this->EvaluateScript(
        base::StringPrintf("test.%sProperty = -9007199254740991;",
                           TypeParam::type_string()),
        NULL));
  }

  // Send 9223372036854775000 (between 2^53 and int64_t max) to
  // 9223372036854774784.
  EXPECT_CALL(this->test_mock(), mock_set_property(9223372036854774784));
  EXPECT_TRUE(this->EvaluateScript(
      base::StringPrintf("test.%sProperty = 9223372036854775000;",
                         TypeParam::type_string()),
      NULL));

  // Unsigned : send 18446744073709550000 (between 2^53
  // and uint64_t max) to 18446744073709549568.
  if (TypeParam::min_value() >= 0) {
    EXPECT_CALL(this->test_mock(), mock_set_property(18446744073709549568ull));
    EXPECT_TRUE(this->EvaluateScript(
        base::StringPrintf("test.%sProperty = 18446744073709550000;",
                           TypeParam::type_string()),
        NULL));
  }
}

// These tests also rely on FromJSValue (similar to above).
TYPED_TEST(LargeIntegerTypeBindingsTest, ArgumentOperationRange) {
  InSequence in_sequence_dummy;

  // Exactly preserve 0.
  EXPECT_CALL(this->test_mock(), MockArgumentOperation(0));
  EXPECT_TRUE(
      this->EvaluateScript(base::StringPrintf("test.%sArgumentOperation(0);",
                                              TypeParam::type_string()),
                           NULL));

  // Exactly preserve 2^53 - 1.
  EXPECT_CALL(this->test_mock(), MockArgumentOperation((1ll << 53) - 1));
  EXPECT_TRUE(this->EvaluateScript(
      base::StringPrintf("test.%sArgumentOperation(9007199254740991);",
                         TypeParam::type_string()),
      NULL));

  // Signed : exactly preserve -(2^53 - 1).
  if (TypeParam::min_value() < 0) {
    EXPECT_CALL(this->test_mock(), MockArgumentOperation(-((1ll << 53) - 1)));
    EXPECT_TRUE(this->EvaluateScript(
        base::StringPrintf("test.%sArgumentOperation(-9007199254740991);",
                           TypeParam::type_string()),
        NULL));
  }

  // Send 9223372036854775000 (between 2^53 and int64_t max) to
  // 9223372036854774784.
  EXPECT_CALL(this->test_mock(), MockArgumentOperation(9223372036854774784));
  EXPECT_TRUE(this->EvaluateScript(
      base::StringPrintf("test.%sArgumentOperation(9223372036854775000);",
                         TypeParam::type_string()),
      NULL));

  // Unsigned : send 18446744073709550000 (between 2^53
  // and uint64_t max) to 18446744073709549568.
  if (TypeParam::min_value() >= 0) {
    EXPECT_CALL(this->test_mock(),
                MockArgumentOperation(18446744073709549568ull));
    EXPECT_TRUE(this->EvaluateScript(
        base::StringPrintf("test.%sArgumentOperation(18446744073709550000);",
                           TypeParam::type_string()),
        NULL));
  }
}
#endif  // ENGINE_SUPPORTS_INT64

TYPED_TEST(FloatingPointTypeBindingsTest, NonFiniteValues) {
  InSequence in_sequence_dummy;
  if (TypeParam::is_restricted()) {
    EXPECT_FALSE(
        this->EvaluateScript(base::StringPrintf("test.%sProperty = Infinity;",
                                                TypeParam::type_string()),
                             NULL));
    EXPECT_FALSE(
        this->EvaluateScript(base::StringPrintf("test.%sProperty = -Infinity;",
                                                TypeParam::type_string()),
                             NULL));
    EXPECT_FALSE(this->EvaluateScript(
        base::StringPrintf("test.%sProperty = NaN;", TypeParam::type_string()),
        NULL));
  } else {
    EXPECT_CALL(this->test_mock(),
                mock_set_property(TypeParam::positive_infinity()));
    EXPECT_TRUE(
        this->EvaluateScript(base::StringPrintf("test.%sProperty = Infinity;",
                                                TypeParam::type_string()),
                             NULL));

    EXPECT_CALL(this->test_mock(),
                mock_set_property(TypeParam::negative_infinity()));
    EXPECT_TRUE(
        this->EvaluateScript(base::StringPrintf("test.%sProperty = -Infinity;",
                                                TypeParam::type_string()),
                             NULL));

    EXPECT_CALL(this->test_mock(),
                mock_set_property(
                    ResultOf(IsNan<typename TypeParam::BaseType>, Eq(true))));
    EXPECT_TRUE(this->EvaluateScript(
        base::StringPrintf("test.%sProperty = NaN;", TypeParam::type_string()),
        NULL));
  }
}

}  // namespace testing
}  // namespace bindings
}  // namespace cobalt

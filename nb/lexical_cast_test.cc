/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "nb/lexical_cast.h"
#include "starboard/types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nb {
namespace {

TEST(lexical_cast, OptionalParameterOmitted) {
  EXPECT_EQ(123, lexical_cast<int>("123"));
  EXPECT_EQ(0, lexical_cast<int>("not a number"));
  EXPECT_EQ(-123, lexical_cast<int8_t>("-123"));
  EXPECT_EQ(123, lexical_cast<uint8_t>("123"));
}

TEST(lexical_cast, PositiveBasicTypes) {
  bool cast_ok = false;
  EXPECT_EQ(123, lexical_cast<int>("123", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(123, lexical_cast<int8_t>("123", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(123, lexical_cast<int16_t>("123", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(123, lexical_cast<int32_t>("123", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(123, lexical_cast<int64_t>("123", &cast_ok));
  EXPECT_TRUE(cast_ok);

  EXPECT_EQ(123, lexical_cast<uint8_t>("123", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(123, lexical_cast<uint16_t>("123", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(123, lexical_cast<uint32_t>("123", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(123, lexical_cast<uint64_t>("123", &cast_ok));
  EXPECT_TRUE(cast_ok);

  EXPECT_FLOAT_EQ(1234.5f, lexical_cast<float>("1234.5", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_FLOAT_EQ(1234.5f, lexical_cast<float>("1234.5f", &cast_ok));
  EXPECT_TRUE(cast_ok);

  EXPECT_FLOAT_EQ(1234.5f, lexical_cast<double>("1234.5", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_FLOAT_EQ(1234.5f, lexical_cast<double>("1234.5f", &cast_ok));
  EXPECT_TRUE(cast_ok);
}

TEST(lexical_cast, NegativeBasicTypes) {
  bool cast_ok = false;
  EXPECT_EQ(-123, lexical_cast<int>("-123", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(-123, lexical_cast<int8_t>("-123", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(-123, lexical_cast<int16_t>("-123", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(-123, lexical_cast<int32_t>("-123", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(-123, lexical_cast<int64_t>("-123", &cast_ok));
  EXPECT_TRUE(cast_ok);

  EXPECT_EQ(0, lexical_cast<uint8_t>("-123", &cast_ok));
  EXPECT_FALSE(cast_ok);
  EXPECT_EQ(0, lexical_cast<uint16_t>("-123", &cast_ok));
  EXPECT_FALSE(cast_ok);
  EXPECT_EQ(0, lexical_cast<uint32_t>("-123", &cast_ok));
  EXPECT_FALSE(cast_ok);
  EXPECT_EQ(0, lexical_cast<uint64_t>("-123", &cast_ok));
  EXPECT_FALSE(cast_ok);

  EXPECT_FLOAT_EQ(-1234.5f, lexical_cast<float>("-1234.5", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_FLOAT_EQ(-1234.5f, lexical_cast<float>("-1234.5f", &cast_ok));
  EXPECT_TRUE(cast_ok);

  EXPECT_FLOAT_EQ(-1234.5f, lexical_cast<double>("-1234.5", &cast_ok));
  EXPECT_TRUE(cast_ok);
  EXPECT_FLOAT_EQ(-1234.5f, lexical_cast<double>("-1234.5f", &cast_ok));
  EXPECT_TRUE(cast_ok);
}

TEST(lexical_cast, StringIsNonNumerical) {
  bool cast_ok = false;
  EXPECT_EQ(0, lexical_cast<int>("not a number", &cast_ok));
  EXPECT_FALSE(cast_ok);
  EXPECT_EQ(0, lexical_cast<int8_t>("not a number", &cast_ok));
  EXPECT_FALSE(cast_ok);
  EXPECT_EQ(0, lexical_cast<int16_t>("not a number", &cast_ok));
  EXPECT_FALSE(cast_ok);
  EXPECT_EQ(0, lexical_cast<int32_t>("not a number", &cast_ok));
  EXPECT_FALSE(cast_ok);
  EXPECT_EQ(0, lexical_cast<int64_t>("not a number", &cast_ok));
  EXPECT_FALSE(cast_ok);

  EXPECT_FLOAT_EQ(0.f, lexical_cast<float>("not a number", &cast_ok));
  EXPECT_FALSE(cast_ok);
  EXPECT_FLOAT_EQ(0.0, lexical_cast<double>("not a number", &cast_ok));
  EXPECT_FALSE(cast_ok);
}

TEST(lexical_cast, StringIsEmpty) {
  bool cast_ok = false;
  int value = lexical_cast<int>("");
  EXPECT_EQ(value, 0);
  EXPECT_FALSE(cast_ok);
}

TEST(lexical_cast, StringIsNull) {
  bool cast_ok = false;
  int value = lexical_cast<int>(NULL);
  EXPECT_EQ(value, 0);
  EXPECT_FALSE(cast_ok);
}

TEST(lexical_cast, IntegerOverflow_int8) {
  bool cast_ok = false;
  int8_t value = lexical_cast<int8_t>("128", &cast_ok);

  EXPECT_FALSE(cast_ok);
  EXPECT_EQ(0, value);
}

TEST(lexical_cast, IntegerOverflow_uint8) {
  bool cast_ok = false;
  uint8_t value = lexical_cast<uint8_t>("256", &cast_ok);

  EXPECT_FALSE(cast_ok);
  EXPECT_EQ(0, value);
}

TEST(lexical_cast, IntegerOverflow_int16) {
  bool cast_ok = false;
  int16_t value = lexical_cast<int16_t>("65535", &cast_ok);

  EXPECT_FALSE(cast_ok);
  EXPECT_EQ(0, value);
}

TEST(lexical_cast, IntegerOverflow_uint16) {
  bool cast_ok = false;
  int16_t value = lexical_cast<int16_t>("65536", &cast_ok);

  EXPECT_FALSE(cast_ok);
  EXPECT_EQ(0, value);
}

TEST(lexical_cast, LetterBeforeNumber) {
  bool cast_ok = false;
  int value = lexical_cast<int>("M128", &cast_ok);
  EXPECT_FALSE(cast_ok);
  EXPECT_EQ(0, value);
}

TEST(lexical_cast, LetterInNumber) {
  bool cast_ok = false;
  int value = lexical_cast<int>("12M8", &cast_ok);
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(12, value);
}

TEST(lexical_cast, LetterAfterNumber) {
  bool cast_ok = false;
  int value = lexical_cast<int>("128M", &cast_ok);
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(128, value);
}

TEST(lexical_cast, OnlyBase10) {
  bool cast_ok = false;
  // Expect that the "100000" part of "100000ff" will be parsed. The "ff" part
  // is dropped.
  uint32_t value = lexical_cast<uint32_t>("100000ff", &cast_ok);
  EXPECT_TRUE(cast_ok);
  EXPECT_EQ(100000, value);  //
}

TEST(lexical_cast, StdString) {
  bool cast_ok = false;
  uint32_t value = lexical_cast<uint32_t>(std::string("100000"), &cast_ok);
  EXPECT_EQ(100000, value);
}

}  // namespace
}  // namespace nb

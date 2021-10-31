// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/layout/layout_unit.h"

#include <cstring>
#include <functional>

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace layout {

TEST(LayoutUnitTest, DefaultConstructorOrDestructorInitializesToZero) {
  LayoutUnit value_a(5);
  value_a.~LayoutUnit();
  LayoutUnit* value_b = new (&value_a) LayoutUnit();
  EXPECT_FLOAT_EQ(value_b->toFloat(), 0.0f);
}

TEST(LayoutUnitTest, DefaultConstructorInitializesToZero) {
  union {
    char* bytes;
    LayoutUnit* value;
  } value_data;
  char bytes[sizeof(LayoutUnit)];
  value_data.bytes = bytes;
  // Initialize the variable data with nonzero bytes. Test for each nonzero byte
  // value.
  for (int byte_value = 1; byte_value < 256; ++byte_value) {
    memset(value_data.bytes, byte_value, sizeof(LayoutUnit));
    LayoutUnit* value = new (value_data.bytes) LayoutUnit();
    EXPECT_FLOAT_EQ(value->toFloat(), 0.0f);
    value_data.value->~LayoutUnit();
  }
}

TEST(LayoutUnitTest, IntConstructor) {
  LayoutUnit value(5);
  EXPECT_FLOAT_EQ(value.toFloat(), 5.0f);
}

TEST(LayoutUnitTest, FloatConstructor) {
  LayoutUnit value(5.0f);
  EXPECT_FLOAT_EQ(value.toFloat(), 5.0f);
}

TEST(LayoutUnitTest, CopyConstructor) {
  LayoutUnit value_a(5.0f);
  LayoutUnit value_b(value_a);
  EXPECT_FLOAT_EQ(value_b.toFloat(), 5.0f);
}

TEST(LayoutUnitTest, Swap) {
  LayoutUnit value_a(5.0f);
  LayoutUnit value_b(35.0f);
  value_a.swap(value_b);
  EXPECT_FLOAT_EQ(value_a.toFloat(), 35.0f);
  EXPECT_FLOAT_EQ(value_b.toFloat(), 5.0f);
}

TEST(LayoutUnitTest, Assignment) {
  LayoutUnit value_a(5.0f);
  LayoutUnit value_b;
  LayoutUnit value_c;
  value_c = value_b = value_a;
  EXPECT_FLOAT_EQ(value_b.toFloat(), 5.0f);
  EXPECT_FLOAT_EQ(value_c.toFloat(), 5.0f);
}

TEST(LayoutUnitTest, EqualOperator) {
  EXPECT_TRUE(LayoutUnit(-8192) == LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(-8192) == LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(-8192) == LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(-8192) == LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(-8192) == LayoutUnit(8192));

  EXPECT_FALSE(LayoutUnit(-1) == LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(-1) == LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(-1) == LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(-1) == LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(-1) == LayoutUnit(8192));

  EXPECT_FALSE(LayoutUnit(0) == LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(0) == LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(0) == LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(0) == LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(0) == LayoutUnit(8192));

  EXPECT_FALSE(LayoutUnit(1) == LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(1) == LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(1) == LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(1) == LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(1) == LayoutUnit(8192));

  EXPECT_FALSE(LayoutUnit(8192) == LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(8192) == LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(8192) == LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(8192) == LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(8192) == LayoutUnit(8192));
}

TEST(LayoutUnitTest, NotEqualOperator) {
  EXPECT_FALSE(LayoutUnit(-8192) != LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(-8192) != LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(-8192) != LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(-8192) != LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(-8192) != LayoutUnit(8192));

  EXPECT_TRUE(LayoutUnit(-1) != LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(-1) != LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(-1) != LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(-1) != LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(-1) != LayoutUnit(8192));

  EXPECT_TRUE(LayoutUnit(0) != LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(0) != LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(0) != LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(0) != LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(0) != LayoutUnit(8192));

  EXPECT_TRUE(LayoutUnit(1) != LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(1) != LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(1) != LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(1) != LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(1) != LayoutUnit(8192));

  EXPECT_TRUE(LayoutUnit(8192) != LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(8192) != LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(8192) != LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(8192) != LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(8192) != LayoutUnit(8192));
}

TEST(LayoutUnitTest, GreaterEqualOperator) {
  EXPECT_TRUE(LayoutUnit(-8192) >= LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(-8192) >= LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(-8192) >= LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(-8192) >= LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(-8192) >= LayoutUnit(8192));

  EXPECT_TRUE(LayoutUnit(-1) >= LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(-1) >= LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(-1) >= LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(-1) >= LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(-1) >= LayoutUnit(8192));

  EXPECT_TRUE(LayoutUnit(0) >= LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(0) >= LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(0) >= LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(0) >= LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(0) >= LayoutUnit(8192));

  EXPECT_TRUE(LayoutUnit(1) >= LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(1) >= LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(1) >= LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(1) >= LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(1) >= LayoutUnit(8192));

  EXPECT_TRUE(LayoutUnit(8192) >= LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(8192) >= LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(8192) >= LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(8192) >= LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(8192) >= LayoutUnit(8192));
}

TEST(LayoutUnitTest, LessEqualOperator) {
  EXPECT_TRUE(LayoutUnit(-8192) <= LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(-8192) <= LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(-8192) <= LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(-8192) <= LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(-8192) <= LayoutUnit(8192));

  EXPECT_FALSE(LayoutUnit(-1) <= LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(-1) <= LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(-1) <= LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(-1) <= LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(-1) <= LayoutUnit(8192));

  EXPECT_FALSE(LayoutUnit(0) <= LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(0) <= LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(0) <= LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(0) <= LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(0) <= LayoutUnit(8192));

  EXPECT_FALSE(LayoutUnit(1) <= LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(1) <= LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(1) <= LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(1) <= LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(1) <= LayoutUnit(8192));

  EXPECT_FALSE(LayoutUnit(8192) <= LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(8192) <= LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(8192) <= LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(8192) <= LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(8192) <= LayoutUnit(8192));
}

TEST(LayoutUnitTest, GreaterOperator) {
  EXPECT_FALSE(LayoutUnit(-8192) > LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(-8192) > LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(-8192) > LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(-8192) > LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(-8192) > LayoutUnit(8192));

  EXPECT_TRUE(LayoutUnit(-1) > LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(-1) > LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(-1) > LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(-1) > LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(-1) > LayoutUnit(8192));

  EXPECT_TRUE(LayoutUnit(0) > LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(0) > LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(0) > LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(0) > LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(0) > LayoutUnit(8192));

  EXPECT_TRUE(LayoutUnit(1) > LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(1) > LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(1) > LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(1) > LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(1) > LayoutUnit(8192));

  EXPECT_TRUE(LayoutUnit(8192) > LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(8192) > LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(8192) > LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(8192) > LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(8192) > LayoutUnit(8192));
}

TEST(LayoutUnitTest, LessOperator) {
  EXPECT_FALSE(LayoutUnit(-8192) < LayoutUnit(-8192));
  EXPECT_TRUE(LayoutUnit(-8192) < LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(-8192) < LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(-8192) < LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(-8192) < LayoutUnit(8192));

  EXPECT_FALSE(LayoutUnit(-1) < LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(-1) < LayoutUnit(-1));
  EXPECT_TRUE(LayoutUnit(-1) < LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(-1) < LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(-1) < LayoutUnit(8192));

  EXPECT_FALSE(LayoutUnit(0) < LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(0) < LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(0) < LayoutUnit(0));
  EXPECT_TRUE(LayoutUnit(0) < LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(0) < LayoutUnit(8192));

  EXPECT_FALSE(LayoutUnit(1) < LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(1) < LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(1) < LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(1) < LayoutUnit(1));
  EXPECT_TRUE(LayoutUnit(1) < LayoutUnit(8192));

  EXPECT_FALSE(LayoutUnit(8192) < LayoutUnit(-8192));
  EXPECT_FALSE(LayoutUnit(8192) < LayoutUnit(-1));
  EXPECT_FALSE(LayoutUnit(8192) < LayoutUnit(0));
  EXPECT_FALSE(LayoutUnit(8192) < LayoutUnit(1));
  EXPECT_FALSE(LayoutUnit(8192) < LayoutUnit(8192));
}

TEST(LayoutUnitTest, PlusOperator) {
  LayoutUnit value_a1(-5.0f);
  LayoutUnit value_b1(5.0f);
  LayoutUnit value_a2 = +value_a1;
  LayoutUnit value_b2 = +value_b1;

  EXPECT_FLOAT_EQ(value_a1.toFloat(), -5.0f);
  EXPECT_FLOAT_EQ(value_b1.toFloat(), 5.0f);
  EXPECT_FLOAT_EQ(value_a2.toFloat(), -5.0f);
  EXPECT_FLOAT_EQ(value_b2.toFloat(), 5.0f);
}

TEST(LayoutUnitTest, MinusOperator) {
  LayoutUnit value_a1(-5.0f);
  LayoutUnit value_b1(5.0f);
  LayoutUnit value_a2 = -value_a1;
  LayoutUnit value_b2 = -value_b1;

  EXPECT_FLOAT_EQ(value_a1.toFloat(), -5.0f);
  EXPECT_FLOAT_EQ(value_b1.toFloat(), 5.0f);
  EXPECT_FLOAT_EQ(value_a2.toFloat(), 5.0f);
  EXPECT_FLOAT_EQ(value_b2.toFloat(), -5.0f);
}

TEST(LayoutUnitTest, AdditionAssignmentOperator) {
  LayoutUnit value_a(5.0f);
  LayoutUnit value_b(10.0f);
  value_a += value_b;

  EXPECT_FLOAT_EQ(value_b.toFloat(), 10.0f);
  EXPECT_FLOAT_EQ(value_a.toFloat(), 15.0f);
}

TEST(LayoutUnitTest, AdditionOperator) {
  EXPECT_EQ(LayoutUnit(-8192) + LayoutUnit(-8192), LayoutUnit(-8192 + -8192));
  EXPECT_EQ(LayoutUnit(-8192) + LayoutUnit(-1), LayoutUnit(-8192 + -1));
  EXPECT_EQ(LayoutUnit(-8192) + LayoutUnit(0), LayoutUnit(-8192 + 0));
  EXPECT_EQ(LayoutUnit(-8192) + LayoutUnit(1), LayoutUnit(-8192 + 1));
  EXPECT_EQ(LayoutUnit(-8192) + LayoutUnit(8192), LayoutUnit(-8192 + 8192));

  EXPECT_EQ(LayoutUnit(-1) + LayoutUnit(-8192), LayoutUnit(-1 + -8192));
  EXPECT_EQ(LayoutUnit(-1) + LayoutUnit(-1), LayoutUnit(-1 + -1));
  EXPECT_EQ(LayoutUnit(-1) + LayoutUnit(0), LayoutUnit(-1 + 0));
  EXPECT_EQ(LayoutUnit(-1) + LayoutUnit(1), LayoutUnit(-1 + 1));
  EXPECT_EQ(LayoutUnit(-1) + LayoutUnit(8192), LayoutUnit(-1 + 8192));

  EXPECT_EQ(LayoutUnit(0) + LayoutUnit(-8192), LayoutUnit(0 + -8192));
  EXPECT_EQ(LayoutUnit(0) + LayoutUnit(-1), LayoutUnit(0 + -1));
  EXPECT_EQ(LayoutUnit(0) + LayoutUnit(0), LayoutUnit(0 + 0));
  EXPECT_EQ(LayoutUnit(0) + LayoutUnit(1), LayoutUnit(0 + 1));
  EXPECT_EQ(LayoutUnit(0) + LayoutUnit(8192), LayoutUnit(0 + 8192));

  EXPECT_EQ(LayoutUnit(1) + LayoutUnit(-8192), LayoutUnit(1 + -8192));
  EXPECT_EQ(LayoutUnit(1) + LayoutUnit(-1), LayoutUnit(1 + -1));
  EXPECT_EQ(LayoutUnit(1) + LayoutUnit(0), LayoutUnit(1 + 0));
  EXPECT_EQ(LayoutUnit(1) + LayoutUnit(1), LayoutUnit(1 + 1));
  EXPECT_EQ(LayoutUnit(1) + LayoutUnit(8192), LayoutUnit(1 + 8192));

  EXPECT_EQ(LayoutUnit(8192) + LayoutUnit(-8192), LayoutUnit(8192 + -8192));
  EXPECT_EQ(LayoutUnit(8192) + LayoutUnit(-1), LayoutUnit(8192 + -1));
  EXPECT_EQ(LayoutUnit(8192) + LayoutUnit(0), LayoutUnit(8192 + 0));
  EXPECT_EQ(LayoutUnit(8192) + LayoutUnit(1), LayoutUnit(8192 + 1));
  EXPECT_EQ(LayoutUnit(8192) + LayoutUnit(8192), LayoutUnit(8192 + 8192));
}

TEST(LayoutUnitTest, SubtractionAssignmentOperator) {
  LayoutUnit value_a(5.0f);
  LayoutUnit value_b(10.0f);
  value_a -= value_b;

  EXPECT_FLOAT_EQ(value_b.toFloat(), 10.0f);
  EXPECT_FLOAT_EQ(value_a.toFloat(), -5.0f);
}

TEST(LayoutUnitTest, SubtractionOperator) {
  EXPECT_EQ(LayoutUnit(-8192) - LayoutUnit(-8192), LayoutUnit(-8192 - -8192));
  EXPECT_EQ(LayoutUnit(-8192) - LayoutUnit(-1), LayoutUnit(-8192 - -1));
  EXPECT_EQ(LayoutUnit(-8192) - LayoutUnit(0), LayoutUnit(-8192 - 0));
  EXPECT_EQ(LayoutUnit(-8192) - LayoutUnit(1), LayoutUnit(-8192 - 1));
  EXPECT_EQ(LayoutUnit(-8192) - LayoutUnit(8192), LayoutUnit(-8192 - 8192));

  EXPECT_EQ(LayoutUnit(-1) - LayoutUnit(-8192), LayoutUnit(-1 - -8192));
  EXPECT_EQ(LayoutUnit(-1) - LayoutUnit(-1), LayoutUnit(-1 - -1));
  EXPECT_EQ(LayoutUnit(-1) - LayoutUnit(0), LayoutUnit(-1 - 0));
  EXPECT_EQ(LayoutUnit(-1) - LayoutUnit(1), LayoutUnit(-1 - 1));
  EXPECT_EQ(LayoutUnit(-1) - LayoutUnit(8192), LayoutUnit(-1 - 8192));

  EXPECT_EQ(LayoutUnit(0) - LayoutUnit(-8192), LayoutUnit(0 - -8192));
  EXPECT_EQ(LayoutUnit(0) - LayoutUnit(-1), LayoutUnit(0 - -1));
  EXPECT_EQ(LayoutUnit(0) - LayoutUnit(0), LayoutUnit(0 - 0));
  EXPECT_EQ(LayoutUnit(0) - LayoutUnit(1), LayoutUnit(0 - 1));
  EXPECT_EQ(LayoutUnit(0) - LayoutUnit(8192), LayoutUnit(0 - 8192));

  EXPECT_EQ(LayoutUnit(1) - LayoutUnit(-8192), LayoutUnit(1 - -8192));
  EXPECT_EQ(LayoutUnit(1) - LayoutUnit(-1), LayoutUnit(1 - -1));
  EXPECT_EQ(LayoutUnit(1) - LayoutUnit(0), LayoutUnit(1 - 0));
  EXPECT_EQ(LayoutUnit(1) - LayoutUnit(1), LayoutUnit(1 - 1));
  EXPECT_EQ(LayoutUnit(1) - LayoutUnit(8192), LayoutUnit(1 - 8192));

  EXPECT_EQ(LayoutUnit(8192) - LayoutUnit(-8192), LayoutUnit(8192 - -8192));
  EXPECT_EQ(LayoutUnit(8192) - LayoutUnit(-1), LayoutUnit(8192 - -1));
  EXPECT_EQ(LayoutUnit(8192) - LayoutUnit(0), LayoutUnit(8192 - 0));
  EXPECT_EQ(LayoutUnit(8192) - LayoutUnit(1), LayoutUnit(8192 - 1));
  EXPECT_EQ(LayoutUnit(8192) - LayoutUnit(8192), LayoutUnit(8192 - 8192));
}

// Integer scaling math operators.

TEST(LayoutUnitTest, IntegerMultiplicationAssignmentOperator) {
  LayoutUnit value_a(5.0f);
  int value_b = 10;
  value_a *= value_b;

  EXPECT_EQ(value_b, 10);
  EXPECT_FLOAT_EQ(value_a.toFloat(), 50.0f);
}

TEST(LayoutUnitTest, IntegerMultiplicationOperator) {
  EXPECT_EQ(LayoutUnit(-128) * -128, LayoutUnit(-128 * -128));
  EXPECT_EQ(LayoutUnit(-128) * -8, LayoutUnit(-128 * -8));
  EXPECT_EQ(LayoutUnit(-128) * 0, LayoutUnit(-128 * 0));
  EXPECT_EQ(LayoutUnit(-128) * 8, LayoutUnit(-128 * 8));
  EXPECT_EQ(LayoutUnit(-128) * 128, LayoutUnit(-128 * 128));

  EXPECT_EQ(LayoutUnit(-8) * -128, LayoutUnit(-8 * -128));
  EXPECT_EQ(LayoutUnit(-8) * -8, LayoutUnit(-8 * -8));
  EXPECT_EQ(LayoutUnit(-8) * 0, LayoutUnit(-8 * 0));
  EXPECT_EQ(LayoutUnit(-8) * 8, LayoutUnit(-8 * 8));
  EXPECT_EQ(LayoutUnit(-8) * 128, LayoutUnit(-8 * 128));

  EXPECT_EQ(LayoutUnit(0) * -128, LayoutUnit(0 * -128));
  EXPECT_EQ(LayoutUnit(0) * -8, LayoutUnit(0 * -8));
  EXPECT_EQ(LayoutUnit(0) * 0, LayoutUnit(0 * 0));
  EXPECT_EQ(LayoutUnit(0) * 8, LayoutUnit(0 * 8));
  EXPECT_EQ(LayoutUnit(0) * 128, LayoutUnit(0 * 128));

  EXPECT_EQ(LayoutUnit(8) * -128, LayoutUnit(8 * -128));
  EXPECT_EQ(LayoutUnit(8) * -8, LayoutUnit(8 * -8));
  EXPECT_EQ(LayoutUnit(8) * 0, LayoutUnit(8 * 0));
  EXPECT_EQ(LayoutUnit(8) * 8, LayoutUnit(8 * 8));
  EXPECT_EQ(LayoutUnit(8) * 128, LayoutUnit(8 * 128));

  EXPECT_EQ(LayoutUnit(128) * -128, LayoutUnit(128 * -128));
  EXPECT_EQ(LayoutUnit(128) * -8, LayoutUnit(128 * -8));
  EXPECT_EQ(LayoutUnit(128) * 0, LayoutUnit(128 * 0));
  EXPECT_EQ(LayoutUnit(128) * 8, LayoutUnit(128 * 8));
  EXPECT_EQ(LayoutUnit(128) * 128, LayoutUnit(128 * 128));

  EXPECT_EQ(-128 * LayoutUnit(-128), LayoutUnit(-128 * -128));
  EXPECT_EQ(-128 * LayoutUnit(-8), LayoutUnit(-128 * -8));
  EXPECT_EQ(-128 * LayoutUnit(0), LayoutUnit(-128 * 0));
  EXPECT_EQ(-128 * LayoutUnit(8), LayoutUnit(-128 * 8));
  EXPECT_EQ(-128 * LayoutUnit(128), LayoutUnit(-128 * 128));

  EXPECT_EQ(-8 * LayoutUnit(-128), LayoutUnit(-8 * -128));
  EXPECT_EQ(-8 * LayoutUnit(-8), LayoutUnit(-8 * -8));
  EXPECT_EQ(-8 * LayoutUnit(0), LayoutUnit(-8 * 0));
  EXPECT_EQ(-8 * LayoutUnit(8), LayoutUnit(-8 * 8));
  EXPECT_EQ(-8 * LayoutUnit(128), LayoutUnit(-8 * 128));

  EXPECT_EQ(0 * LayoutUnit(-128), LayoutUnit(0 * -128));
  EXPECT_EQ(0 * LayoutUnit(-8), LayoutUnit(0 * -8));
  EXPECT_EQ(0 * LayoutUnit(0), LayoutUnit(0 * 0));
  EXPECT_EQ(0 * LayoutUnit(8), LayoutUnit(0 * 8));
  EXPECT_EQ(0 * LayoutUnit(128), LayoutUnit(0 * 128));

  EXPECT_EQ(8 * LayoutUnit(-128), LayoutUnit(8 * -128));
  EXPECT_EQ(8 * LayoutUnit(-8), LayoutUnit(8 * -8));
  EXPECT_EQ(8 * LayoutUnit(0), LayoutUnit(8 * 0));
  EXPECT_EQ(8 * LayoutUnit(8), LayoutUnit(8 * 8));
  EXPECT_EQ(8 * LayoutUnit(128), LayoutUnit(8 * 128));

  EXPECT_EQ(128 * LayoutUnit(-128), LayoutUnit(128 * -128));
  EXPECT_EQ(128 * LayoutUnit(-8), LayoutUnit(128 * -8));
  EXPECT_EQ(128 * LayoutUnit(0), LayoutUnit(128 * 0));
  EXPECT_EQ(128 * LayoutUnit(8), LayoutUnit(128 * 8));
  EXPECT_EQ(128 * LayoutUnit(128), LayoutUnit(128 * 128));
}

TEST(LayoutUnitTest, IntegerDivisionAssignmentOperator) {
  LayoutUnit value_a(50.0f);
  int value_b = 10;
  value_a /= value_b;

  EXPECT_EQ(value_b, 10);
  EXPECT_FLOAT_EQ(value_a.toFloat(), 5.0f);
}

TEST(LayoutUnitTest, IntegerDivisionOperator) {
  EXPECT_EQ(LayoutUnit(-64) / -64, LayoutUnit(-64.0f / -64));
  EXPECT_EQ(LayoutUnit(-64) / -4, LayoutUnit(-64.0f / -4));
  EXPECT_EQ(LayoutUnit(-64) / 1, LayoutUnit(-64.0f / 1));
  EXPECT_EQ(LayoutUnit(-64) / 4, LayoutUnit(-64.0f / 4));
  EXPECT_EQ(LayoutUnit(-64) / 64, LayoutUnit(-64.0f / 64));

  EXPECT_EQ(LayoutUnit(-4) / -64, LayoutUnit(-4.0f / -64));
  EXPECT_EQ(LayoutUnit(-4) / -4, LayoutUnit(-4.0f / -4));
  EXPECT_EQ(LayoutUnit(-4) / 1, LayoutUnit(-4.0f / 1));
  EXPECT_EQ(LayoutUnit(-4) / 4, LayoutUnit(-4.0f / 4));
  EXPECT_EQ(LayoutUnit(-4) / 64, LayoutUnit(-4.0f / 64));

  EXPECT_EQ(LayoutUnit(1) / -64, LayoutUnit(1.0f / -64));
  EXPECT_EQ(LayoutUnit(1) / -4, LayoutUnit(1.0f / -4));
  EXPECT_EQ(LayoutUnit(1) / 1, LayoutUnit(1.0f / 1));
  EXPECT_EQ(LayoutUnit(1) / 4, LayoutUnit(1.0f / 4));
  EXPECT_EQ(LayoutUnit(1) / 64, LayoutUnit(1.0f / 64));

  EXPECT_EQ(LayoutUnit(4) / -64, LayoutUnit(4.0f / -64));
  EXPECT_EQ(LayoutUnit(4) / -4, LayoutUnit(4.0f / -4));
  EXPECT_EQ(LayoutUnit(4) / 1, LayoutUnit(4.0f / 1));
  EXPECT_EQ(LayoutUnit(4) / 4, LayoutUnit(4.0f / 4));
  EXPECT_EQ(LayoutUnit(4) / 64, LayoutUnit(4.0f / 64));

  EXPECT_EQ(LayoutUnit(64) / -64, LayoutUnit(64.0f / -64));
  EXPECT_EQ(LayoutUnit(64) / -4, LayoutUnit(64.0f / -4));
  EXPECT_EQ(LayoutUnit(64) / 1, LayoutUnit(64.0f / 1));
  EXPECT_EQ(LayoutUnit(64) / 4, LayoutUnit(64.0f / 4));
  EXPECT_EQ(LayoutUnit(64) / 64, LayoutUnit(64.0f / 64));
}

// Floating point scaling math operators.

TEST(LayoutUnitTest, FloatingPointMultiplicationAssignmentOperator) {
  LayoutUnit value_a(5.0f);
  float value_b = 10.0f;
  value_a *= value_b;

  EXPECT_FLOAT_EQ(value_b, 10.0f);
  EXPECT_FLOAT_EQ(value_a.toFloat(), 50.0f);
}

TEST(LayoutUnitTest, FloatingPointMultiplicationOperator) {
  EXPECT_EQ(LayoutUnit(-128) * -128.0f, LayoutUnit(-128 * -128));
  EXPECT_EQ(LayoutUnit(-128) * -8.0f, LayoutUnit(-128 * -8));
  EXPECT_EQ(LayoutUnit(-128) * 0.0f, LayoutUnit(-128 * 0));
  EXPECT_EQ(LayoutUnit(-128) * 8.0f, LayoutUnit(-128 * 8));
  EXPECT_EQ(LayoutUnit(-128) * 128.0f, LayoutUnit(-128 * 128));

  EXPECT_EQ(LayoutUnit(-8) * -128.0f, LayoutUnit(-8 * -128));
  EXPECT_EQ(LayoutUnit(-8) * -8.0f, LayoutUnit(-8 * -8));
  EXPECT_EQ(LayoutUnit(-8) * 0.0f, LayoutUnit(-8 * 0));
  EXPECT_EQ(LayoutUnit(-8) * 8.0f, LayoutUnit(-8 * 8));
  EXPECT_EQ(LayoutUnit(-8) * 128.0f, LayoutUnit(-8 * 128));

  EXPECT_EQ(LayoutUnit(0) * -128.0f, LayoutUnit(0 * -128));
  EXPECT_EQ(LayoutUnit(0) * -8.0f, LayoutUnit(0 * -8));
  EXPECT_EQ(LayoutUnit(0) * 0.0f, LayoutUnit(0 * 0));
  EXPECT_EQ(LayoutUnit(0) * 8.0f, LayoutUnit(0 * 8));
  EXPECT_EQ(LayoutUnit(0) * 128.0f, LayoutUnit(0 * 128));

  EXPECT_EQ(LayoutUnit(8) * -128.0f, LayoutUnit(8 * -128));
  EXPECT_EQ(LayoutUnit(8) * -8.0f, LayoutUnit(8 * -8));
  EXPECT_EQ(LayoutUnit(8) * 0.0f, LayoutUnit(8 * 0));
  EXPECT_EQ(LayoutUnit(8) * 8.0f, LayoutUnit(8 * 8));
  EXPECT_EQ(LayoutUnit(8) * 128.0f, LayoutUnit(8 * 128));

  EXPECT_EQ(LayoutUnit(128) * -128.0f, LayoutUnit(128 * -128));
  EXPECT_EQ(LayoutUnit(128) * -8.0f, LayoutUnit(128 * -8));
  EXPECT_EQ(LayoutUnit(128) * 0.0f, LayoutUnit(128 * 0));
  EXPECT_EQ(LayoutUnit(128) * 8.0f, LayoutUnit(128 * 8));
  EXPECT_EQ(LayoutUnit(128) * 128.0f, LayoutUnit(128 * 128));

  EXPECT_EQ(-128.0f * LayoutUnit(-128), LayoutUnit(-128 * -128));
  EXPECT_EQ(-128.0f * LayoutUnit(-8), LayoutUnit(-128 * -8));
  EXPECT_EQ(-128.0f * LayoutUnit(0), LayoutUnit(-128 * 0));
  EXPECT_EQ(-128.0f * LayoutUnit(8), LayoutUnit(-128 * 8));
  EXPECT_EQ(-128.0f * LayoutUnit(128), LayoutUnit(-128 * 128));

  EXPECT_EQ(-8.0f * LayoutUnit(-128), LayoutUnit(-8 * -128));
  EXPECT_EQ(-8.0f * LayoutUnit(-8), LayoutUnit(-8 * -8));
  EXPECT_EQ(-8.0f * LayoutUnit(0), LayoutUnit(-8 * 0));
  EXPECT_EQ(-8.0f * LayoutUnit(8), LayoutUnit(-8 * 8));
  EXPECT_EQ(-8.0f * LayoutUnit(128), LayoutUnit(-8 * 128));

  EXPECT_EQ(0.0f * LayoutUnit(-128), LayoutUnit(0 * -128));
  EXPECT_EQ(0.0f * LayoutUnit(-8), LayoutUnit(0 * -8));
  EXPECT_EQ(0.0f * LayoutUnit(0), LayoutUnit(0 * 0));
  EXPECT_EQ(0.0f * LayoutUnit(8), LayoutUnit(0 * 8));
  EXPECT_EQ(0.0f * LayoutUnit(128), LayoutUnit(0 * 128));

  EXPECT_EQ(8.0f * LayoutUnit(-128), LayoutUnit(8 * -128));
  EXPECT_EQ(8.0f * LayoutUnit(-8), LayoutUnit(8 * -8));
  EXPECT_EQ(8.0f * LayoutUnit(0), LayoutUnit(8 * 0));
  EXPECT_EQ(8.0f * LayoutUnit(8), LayoutUnit(8 * 8));
  EXPECT_EQ(8.0f * LayoutUnit(128), LayoutUnit(8 * 128));

  EXPECT_EQ(128.0f * LayoutUnit(-128), LayoutUnit(128 * -128));
  EXPECT_EQ(128.0f * LayoutUnit(-8), LayoutUnit(128 * -8));
  EXPECT_EQ(128.0f * LayoutUnit(0), LayoutUnit(128 * 0));
  EXPECT_EQ(128.0f * LayoutUnit(8), LayoutUnit(128 * 8));
  EXPECT_EQ(128.0f * LayoutUnit(128), LayoutUnit(128 * 128));
}

TEST(LayoutUnitTest, FloatingPointDivisionAssignmentOperator) {
  LayoutUnit value_a(50.0f);
  float value_b = 10.0f;
  value_a /= value_b;

  EXPECT_FLOAT_EQ(value_b, 10.0f);
  EXPECT_FLOAT_EQ(value_a.toFloat(), 5.0f);
}

TEST(LayoutUnitTest, FloatingPointDivisionOperator) {
  EXPECT_EQ(LayoutUnit(-64) / -64.0f, LayoutUnit(-64.0f / -64.0f));
  EXPECT_EQ(LayoutUnit(-64) / -4.0f, LayoutUnit(-64.0f / -4.0f));
  EXPECT_EQ(LayoutUnit(-64) / 1.0f, LayoutUnit(-64.0f / 1.0f));
  EXPECT_EQ(LayoutUnit(-64) / 4.0f, LayoutUnit(-64.0f / 4.0f));
  EXPECT_EQ(LayoutUnit(-64) / 64.0f, LayoutUnit(-64.0f / 64.0f));

  EXPECT_EQ(LayoutUnit(-4) / -64.0f, LayoutUnit(-4.0f / -64.0f));
  EXPECT_EQ(LayoutUnit(-4) / -4.0f, LayoutUnit(-4.0f / -4.0f));
  EXPECT_EQ(LayoutUnit(-4) / 1.0f, LayoutUnit(-4.0f / 1.0f));
  EXPECT_EQ(LayoutUnit(-4) / 4.0f, LayoutUnit(-4.0f / 4.0f));
  EXPECT_EQ(LayoutUnit(-4) / 64.0f, LayoutUnit(-4.0f / 64.0f));

  EXPECT_EQ(LayoutUnit(1) / -64.0f, LayoutUnit(1.0f / -64.0f));
  EXPECT_EQ(LayoutUnit(1) / -4.0f, LayoutUnit(1.0f / -4.0f));
  EXPECT_EQ(LayoutUnit(1) / 1.0f, LayoutUnit(1.0f / 1.0f));
  EXPECT_EQ(LayoutUnit(1) / 4.0f, LayoutUnit(1.0f / 4.0f));
  EXPECT_EQ(LayoutUnit(1) / 64.0f, LayoutUnit(1.0f / 64.0f));

  EXPECT_EQ(LayoutUnit(4) / -64.0f, LayoutUnit(4.0f / -64.0f));
  EXPECT_EQ(LayoutUnit(4) / -4.0f, LayoutUnit(4.0f / -4.0f));
  EXPECT_EQ(LayoutUnit(4) / 1.0f, LayoutUnit(4.0f / 1.0f));
  EXPECT_EQ(LayoutUnit(4) / 4.0f, LayoutUnit(4.0f / 4.0f));
  EXPECT_EQ(LayoutUnit(4) / 64.0f, LayoutUnit(4.0f / 64.0f));

  EXPECT_EQ(LayoutUnit(64) / -64.0f, LayoutUnit(64.0f / -64.0f));
  EXPECT_EQ(LayoutUnit(64) / -4.0f, LayoutUnit(64.0f / -4.0f));
  EXPECT_EQ(LayoutUnit(64) / 1.0f, LayoutUnit(64.0f / 1.0f));
  EXPECT_EQ(LayoutUnit(64) / 4.0f, LayoutUnit(64.0f / 4.0f));
  EXPECT_EQ(LayoutUnit(64) / 64.0f, LayoutUnit(64.0f / 64.0f));
}

}  // namespace layout
}  // namespace cobalt

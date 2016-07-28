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

#include "cobalt/dom/float64_array.h"

#include <limits>

#include "cobalt/script/testing/mock_exception_state.h"
#include "testing/gtest/include/gtest/gtest.h"

// Note that most of the basic functionalities are already tested inside
// typed_array_test.cc This file only includes tests that are very specific to
// Float64Array.

namespace cobalt {
namespace dom {
namespace {

using script::testing::MockExceptionState;
using testing::StrictMock;

// Sanity check that we can actually store double values and get them back.
TEST(Float64Array, SetGetDouble) {
  static const uint32 kLength = 5;
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<Float64Array> array =
      new Float64Array(NULL, kLength, &exception_state);
  double values[kLength];

  // Use an array to keep the values to avoid any floating point inaccuracy.
  for (uint32 i = 0; i < kLength; ++i) {
    values[i] = static_cast<double>(i) / 10.;
    array->Set(i, values[i]);
  }

  for (uint32 i = 0; i < kLength; ++i) {
    EXPECT_EQ(values[i], array->Get(i));
  }
}

// The Float64Array supports double values like Infinity and NaN.
TEST(Float64Array, UnrestrictedDoubleValues) {
  StrictMock<MockExceptionState> exception_state;
  scoped_refptr<Float64Array> array =
      new Float64Array(NULL, 1, &exception_state);

  array->Set(0, std::numeric_limits<double>::infinity());
  EXPECT_EQ(std::numeric_limits<double>::infinity(), array->Get(0));

  array->Set(0, -std::numeric_limits<double>::infinity());
  EXPECT_EQ(-std::numeric_limits<double>::infinity(), array->Get(0));

  array->Set(0, std::numeric_limits<double>::quiet_NaN());
  // std::isnan() is not available in C++98.  So we compare it against itself
  // to check if it is a NaN as NaN == NaN returns false.
  EXPECT_FALSE(array->Get(0) == array->Get(0));
}

}  // namespace
}  // namespace dom
}  // namespace cobalt

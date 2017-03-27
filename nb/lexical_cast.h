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

#ifndef NB_LEXICAL_CAST_H_
#define NB_LEXICAL_CAST_H_

#include <limits>
#include <sstream>

#include "starboard/types.h"

namespace nb {

// Converts a string into a value. This function should not be used in
// performance sensitive code.
//
// Note:
//  * All strings are assumed to represent numbers in base 10.
//  * Casting will parse until a non-numerical character is encountered:
//    * Numbers like "128M" will drop "M" and cast to a value of 128.
//    * Numbers like "12M8" will cast to value 12.
//    * Numbers like "M128" will fail to cast.
//
// Returns the value of the result after the lexical cast. If the lexical
// cast fails then the default value of the parameterized type is returned.
// |cast_ok| is an optional parameter which will be |true| if the cast
// succeeds, otherwise |false|.
// Example:
//   int value = lexical_cast<int>("1234");
//   EXPECT_EQ(value, 1234);
//   bool ok = true;
//   value = lexical_cast<int>("not a number", &ok);
//   EXPECT_FALSE(ok);
//   EXPECT_EQ(0, value);
template <typename T>
T lexical_cast(const char* s, bool* cast_ok = NULL) {
  if (!s) {  // Handle NULL case of input string.
    if (cast_ok) {
      *cast_ok = false;
    }
    return T();
  }
  std::stringstream ss;
  ss << s;
  T value;
  ss >> value;
  if (cast_ok) {
    *cast_ok = !ss.fail();
  }
  if (ss.fail()) {
    value = T();
  }
  return value;
}

// int8_t and uint8_t will normally be interpreted as a char, which will
// result in only the first character being parsed. This is obviously not
// what we want. Therefore we provide specializations for lexical_cast for
// these types.
template <>
int8_t lexical_cast<int8_t>(const char* s, bool* cast_ok) {
  int16_t value_i16 = lexical_cast<int16_t>(s, cast_ok);
  if (value_i16 < std::numeric_limits<int8_t>::min() ||
      value_i16 > std::numeric_limits<int8_t>::max()) {
    value_i16 = 0;
    if (cast_ok) {
      *cast_ok = false;
    }
  }
  return static_cast<int8_t>(value_i16);
}

template <typename SmallInt>
SmallInt NarrowingLexicalCast(const char* s, bool* cast_ok) {
  int64_t value = lexical_cast<int64_t>(s, cast_ok);
  if ((value > std::numeric_limits<SmallInt>::max()) ||
      (value < std::numeric_limits<SmallInt>::min())) {
    if (cast_ok) {
      *cast_ok = false;
    }
    return static_cast<SmallInt>(0);
  }
  return static_cast<SmallInt>(value);
}

template <>
uint8_t lexical_cast<uint8_t>(const char* s, bool* cast_ok) {
  return NarrowingLexicalCast<uint8_t>(s, cast_ok);
}

template <>
uint16_t lexical_cast<uint16_t>(const char* s, bool* cast_ok) {
  return NarrowingLexicalCast<uint16_t>(s, cast_ok);
}

template <>
uint32_t lexical_cast<uint32_t>(const char* s, bool* cast_ok) {
  return NarrowingLexicalCast<uint32_t>(s, cast_ok);
}

// uint64_t types will have a max value of int64_t. But this is acceptable.
template <>
uint64_t lexical_cast<uint64_t>(const char* s, bool* cast_ok) {
  int64_t val_i64 = lexical_cast<int64_t>(s, cast_ok);
  // Handle failure condition for negative values.
  if (val_i64 < 0) {
    val_i64 = 0;
    if (cast_ok) {
      *cast_ok = false;
    }
  }
  return static_cast<uint64_t>(val_i64);
}

}  // namespace nb

#endif  // NB_LEXICAL_CAST_H_

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>

#include <limits>

#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

template <typename INT>
struct IntToStringTest {
  INT num;
  const char* sexpected;
  const char* uexpected;
};

}  // namespace

TEST(StringNumberConversionsTest, IntToString) {
  static const IntToStringTest<int> int_tests[] = {
      { 0, "0", "0" },
      { -1, "-1", "4294967295" },
      { std::numeric_limits<int>::max(), "2147483647", "2147483647" },
      { std::numeric_limits<int>::min(), "-2147483648", "2147483648" },
  };
  static const IntToStringTest<int64> int64_tests[] = {
      { 0, "0", "0" },
      { -1, "-1", "18446744073709551615" },
      { std::numeric_limits<int64>::max(),
        "9223372036854775807",
        "9223372036854775807", },
      { std::numeric_limits<int64>::min(),
        "-9223372036854775808",
        "9223372036854775808" },
  };

  for (size_t i = 0; i < arraysize(int_tests); ++i) {
    const IntToStringTest<int>* test = &int_tests[i];
    EXPECT_EQ(IntToString(test->num), test->sexpected);
    EXPECT_EQ(IntToString16(test->num), UTF8ToUTF16(test->sexpected));
    EXPECT_EQ(UintToString(test->num), test->uexpected);
    EXPECT_EQ(UintToString16(test->num), UTF8ToUTF16(test->uexpected));
  }
  for (size_t i = 0; i < arraysize(int64_tests); ++i) {
    const IntToStringTest<int64>* test = &int64_tests[i];
    EXPECT_EQ(Int64ToString(test->num), test->sexpected);
    EXPECT_EQ(Int64ToString16(test->num), UTF8ToUTF16(test->sexpected));
    EXPECT_EQ(Uint64ToString(test->num), test->uexpected);
    EXPECT_EQ(Uint64ToString16(test->num), UTF8ToUTF16(test->uexpected));
  }
}

TEST(StringNumberConversionsTest, Uint64ToString) {
  static const struct {
    uint64 input;
    std::string output;
  } cases[] = {
    {0, "0"},
    {42, "42"},
    {INT_MAX, "2147483647"},
    {kuint64max, "18446744073709551615"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i)
    EXPECT_EQ(cases[i].output, Uint64ToString(cases[i].input));
}

TEST(StringNumberConversionsTest, StringToInt) {
  static const struct {
    std::string input;
    int output;
    bool success;
  } cases[] = {
    {"0", 0, true},
    {"42", 42, true},
    {"42\x99", 42, false},
    {"\x99" "42\x99", 0, false},
    {"-2147483648", INT_MIN, true},
    {"2147483647", INT_MAX, true},
    {"", 0, false},
    {" 42", 42, false},
    {"42 ", 42, false},
    {"\t\n\v\f\r 42", 42, false},
    {"blah42", 0, false},
    {"42blah", 42, false},
    {"blah42blah", 0, false},
    {"-273.15", -273, false},
    {"+98.6", 98, false},
    {"--123", 0, false},
    {"++123", 0, false},
    {"-+123", 0, false},
    {"+-123", 0, false},
    {"-", 0, false},
    {"-2147483649", INT_MIN, false},
    {"-99999999999", INT_MIN, false},
    {"2147483648", INT_MAX, false},
    {"99999999999", INT_MAX, false},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    int output = 0;
    EXPECT_EQ(cases[i].success, StringToInt(cases[i].input, &output));
    EXPECT_EQ(cases[i].output, output);

    string16 utf16_input = UTF8ToUTF16(cases[i].input);
    output = 0;
    EXPECT_EQ(cases[i].success, StringToInt(utf16_input, &output));
    EXPECT_EQ(cases[i].output, output);
  }

  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] = "6\06";
  std::string input_string(input, arraysize(input) - 1);
  int output;
  EXPECT_FALSE(StringToInt(input_string, &output));
  EXPECT_EQ(6, output);

  string16 utf16_input = UTF8ToUTF16(input_string);
  output = 0;
  EXPECT_FALSE(StringToInt(utf16_input, &output));
  EXPECT_EQ(6, output);

  output = 0;
  const char16 negative_wide_input[] = { 0xFF4D, '4', '2', 0};
  EXPECT_FALSE(StringToInt(string16(negative_wide_input), &output));
  EXPECT_EQ(0, output);
}

TEST(StringNumberConversionsTest, StringToInt64) {
  static const struct {
    std::string input;
    int64 output;
    bool success;
  } cases[] = {
    {"0", 0, true},
    {"42", 42, true},
    {"-2147483648", INT_MIN, true},
    {"2147483647", INT_MAX, true},
    {"-2147483649", GG_INT64_C(-2147483649), true},
    {"-99999999999", GG_INT64_C(-99999999999), true},
    {"2147483648", GG_INT64_C(2147483648), true},
    {"99999999999", GG_INT64_C(99999999999), true},
    {"9223372036854775807", kint64max, true},
    {"-9223372036854775808", kint64min, true},
    {"09", 9, true},
    {"-09", -9, true},
    {"", 0, false},
    {" 42", 42, false},
    {"42 ", 42, false},
    {"0x42", 0, false},
    {"\t\n\v\f\r 42", 42, false},
    {"blah42", 0, false},
    {"42blah", 42, false},
    {"blah42blah", 0, false},
    {"-273.15", -273, false},
    {"+98.6", 98, false},
    {"--123", 0, false},
    {"++123", 0, false},
    {"-+123", 0, false},
    {"+-123", 0, false},
    {"-", 0, false},
    {"-9223372036854775809", kint64min, false},
    {"-99999999999999999999", kint64min, false},
    {"9223372036854775808", kint64max, false},
    {"99999999999999999999", kint64max, false},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    int64 output = 0;
    EXPECT_EQ(cases[i].success, StringToInt64(cases[i].input, &output));
    EXPECT_EQ(cases[i].output, output);

    string16 utf16_input = UTF8ToUTF16(cases[i].input);
    output = 0;
    EXPECT_EQ(cases[i].success, StringToInt64(utf16_input, &output));
    EXPECT_EQ(cases[i].output, output);
  }

  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] = "6\06";
  std::string input_string(input, arraysize(input) - 1);
  int64 output;
  EXPECT_FALSE(StringToInt64(input_string, &output));
  EXPECT_EQ(6, output);

  string16 utf16_input = UTF8ToUTF16(input_string);
  output = 0;
  EXPECT_FALSE(StringToInt64(utf16_input, &output));
  EXPECT_EQ(6, output);
}

TEST(StringNumberConversionsTest, HexStringToInt) {
  static const struct {
    std::string input;
    int output;
    bool success;
  } cases[] = {
    {"0", 0, true},
    {"42", 66, true},
    {"-42", -66, true},
    {"+42", 66, true},
    {"7fffffff", INT_MAX, true},
    {"80000000", INT_MIN, true},
    {"ffffffff", -1, true},
    {"DeadBeef", 0xdeadbeef, true},
    {"0x42", 66, true},
    {"-0x42", -66, true},
    {"+0x42", 66, true},
    {"0x7fffffff", INT_MAX, true},
    {"0x80000000", INT_MIN, true},
    {"0xffffffff", -1, true},
    {"0XDeadBeef", 0xdeadbeef, true},
    {"0x0f", 15, true},
    {"0f", 15, true},
    {" 45", 0x45, false},
    {"\t\n\v\f\r 0x45", 0x45, false},
    {" 45", 0x45, false},
    {"45 ", 0x45, false},
    {"45:", 0x45, false},
    {"efgh", 0xef, false},
    {"0xefgh", 0xef, false},
    {"hgfe", 0, false},
    {"100000000", -1, false},  // don't care about |output|, just |success|
    {"-", 0, false},
    {"", 0, false},
    {"0x", 0, false},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    int output = 0;
    EXPECT_EQ(cases[i].success, HexStringToInt(cases[i].input, &output));
    EXPECT_EQ(cases[i].output, output);
  }
  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] = "0xc0ffee\09";
  std::string input_string(input, arraysize(input) - 1);
  int output;
  EXPECT_FALSE(HexStringToInt(input_string, &output));
  EXPECT_EQ(0xc0ffee, output);
}

TEST(StringNumberConversionsTest, HexStringToBytes) {
  static const struct {
    const std::string input;
    const char* output;
    size_t output_len;
    bool success;
  } cases[] = {
    {"0", "", 0, false},  // odd number of characters fails
    {"00", "\0", 1, true},
    {"42", "\x42", 1, true},
    {"-42", "", 0, false},  // any non-hex value fails
    {"+42", "", 0, false},
    {"7fffffff", "\x7f\xff\xff\xff", 4, true},
    {"80000000", "\x80\0\0\0", 4, true},
    {"deadbeef", "\xde\xad\xbe\xef", 4, true},
    {"DeadBeef", "\xde\xad\xbe\xef", 4, true},
    {"0x42", "", 0, false},  // leading 0x fails (x is not hex)
    {"0f", "\xf", 1, true},
    {"45  ", "\x45", 1, false},
    {"efgh", "\xef", 1, false},
    {"", "", 0, false},
    {"0123456789ABCDEF", "\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8, true},
    {"0123456789ABCDEF012345",
     "\x01\x23\x45\x67\x89\xAB\xCD\xEF\x01\x23\x45", 11, true},
  };


  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    std::vector<uint8> output;
    std::vector<uint8> compare;
    EXPECT_EQ(cases[i].success, HexStringToBytes(cases[i].input, &output)) <<
        i << ": " << cases[i].input;
    for (size_t j = 0; j < cases[i].output_len; ++j)
      compare.push_back(static_cast<uint8>(cases[i].output[j]));
    ASSERT_EQ(output.size(), compare.size()) << i << ": " << cases[i].input;
    EXPECT_TRUE(std::equal(output.begin(), output.end(), compare.begin())) <<
        i << ": " << cases[i].input;
  }
}

TEST(StringNumberConversionsTest, StringToDouble) {
  static const struct {
    std::string input;
    double output;
    bool success;
  } cases[] = {
    {"0", 0.0, true},
    {"42", 42.0, true},
    {"-42", -42.0, true},
    {"123.45", 123.45, true},
    {"-123.45", -123.45, true},
    {"+123.45", 123.45, true},
    {"2.99792458e8", 299792458.0, true},
    {"149597870.691E+3", 149597870691.0, true},
    {"6.", 6.0, true},
    {"9e99999999999999999999", HUGE_VAL, false},
    {"-9e99999999999999999999", -HUGE_VAL, false},
    {"1e-2", 0.01, true},
    {"42 ", 42.0, false},
    {" 1e-2", 0.01, false},
    {"1e-2 ", 0.01, false},
    {"-1E-7", -0.0000001, true},
    {"01e02", 100, true},
    {"2.3e15", 2.3e15, true},
    {"\t\n\v\f\r -123.45e2", -12345.0, false},
    {"+123 e4", 123.0, false},
    {"123e ", 123.0, false},
    {"123e", 123.0, false},
    {" 2.99", 2.99, false},
    {"1e3.4", 1000.0, false},
    {"nothing", 0.0, false},
    {"-", 0.0, false},
    {"+", 0.0, false},
    {"", 0.0, false},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    double output;
    EXPECT_EQ(cases[i].success, StringToDouble(cases[i].input, &output));
    EXPECT_DOUBLE_EQ(cases[i].output, output);
  }

  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] = "3.14\0159";
  std::string input_string(input, arraysize(input) - 1);
  double output;
  EXPECT_FALSE(StringToDouble(input_string, &output));
  EXPECT_DOUBLE_EQ(3.14, output);
}

TEST(StringNumberConversionsTest, DoubleToString) {
  static const struct {
    double input;
    const char* expected;
  } cases[] = {
    {0.0, "0"},
    {1.25, "1.25"},
    {1.33518e+012, "1.33518e+12"},
    {1.33489e+012, "1.33489e+12"},
    {1.33505e+012, "1.33505e+12"},
    {1.33545e+009, "1335450000"},
    {1.33503e+009, "1335030000"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    EXPECT_EQ(cases[i].expected, DoubleToString(cases[i].input));
  }

  // The following two values were seen in crashes in the wild.
  const char input_bytes[8] = {0, 0, 0, 0, '\xee', '\x6d', '\x73', '\x42'};
  double input = 0;
  memcpy(&input, input_bytes, arraysize(input_bytes));
  EXPECT_EQ("1335179083776", DoubleToString(input));
  const char input_bytes2[8] =
      {0, 0, 0, '\xa0', '\xda', '\x6c', '\x73', '\x42'};
  input = 0;
  memcpy(&input, input_bytes2, arraysize(input_bytes2));
  EXPECT_EQ("1334890332160", DoubleToString(input));
}

TEST(StringNumberConversionsTest, HexEncode) {
  std::string hex(HexEncode(NULL, 0));
  EXPECT_EQ(hex.length(), 0U);
  unsigned char bytes[] = {0x01, 0xff, 0x02, 0xfe, 0x03, 0x80, 0x81};
  hex = HexEncode(bytes, sizeof(bytes));
  EXPECT_EQ(hex.compare("01FF02FE038081"), 0);
}

}  // namespace base

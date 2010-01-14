// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>
#include <stdarg.h>

#include <limits>
#include <sstream>

#include "base/basictypes.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Given a null-terminated string of wchar_t with each wchar_t representing
// a UTF-16 code unit, returns a string16 made up of wchar_t's in the input.
// Each wchar_t should be <= 0xFFFF and a non-BMP character (> U+FFFF)
// should be represented as a surrogate pair (two UTF-16 units)
// *even* where wchar_t is 32-bit (Linux and Mac).
//
// This is to help write tests for functions with string16 params until
// the C++ 0x UTF-16 literal is well-supported by compilers.
string16 BuildString16(const wchar_t* s) {
#if defined(WCHAR_T_IS_UTF16)
  return string16(s);
#elif defined(WCHAR_T_IS_UTF32)
  string16 u16;
  while (*s != 0) {
    DCHECK(static_cast<unsigned int>(*s) <= 0xFFFFu);
    u16.push_back(*s++);
  }
  return u16;
#endif
}

}  // namespace

static const struct trim_case {
  const wchar_t* input;
  const TrimPositions positions;
  const wchar_t* output;
  const TrimPositions return_value;
} trim_cases[] = {
  {L" Google Video ", TRIM_LEADING, L"Google Video ", TRIM_LEADING},
  {L" Google Video ", TRIM_TRAILING, L" Google Video", TRIM_TRAILING},
  {L" Google Video ", TRIM_ALL, L"Google Video", TRIM_ALL},
  {L"Google Video", TRIM_ALL, L"Google Video", TRIM_NONE},
  {L"", TRIM_ALL, L"", TRIM_NONE},
  {L"  ", TRIM_LEADING, L"", TRIM_LEADING},
  {L"  ", TRIM_TRAILING, L"", TRIM_TRAILING},
  {L"  ", TRIM_ALL, L"", TRIM_ALL},
  {L"\t\rTest String\n", TRIM_ALL, L"Test String", TRIM_ALL},
  {L"\x2002Test String\x00A0\x3000", TRIM_ALL, L"Test String", TRIM_ALL},
};

static const struct trim_case_ascii {
  const char* input;
  const TrimPositions positions;
  const char* output;
  const TrimPositions return_value;
} trim_cases_ascii[] = {
  {" Google Video ", TRIM_LEADING, "Google Video ", TRIM_LEADING},
  {" Google Video ", TRIM_TRAILING, " Google Video", TRIM_TRAILING},
  {" Google Video ", TRIM_ALL, "Google Video", TRIM_ALL},
  {"Google Video", TRIM_ALL, "Google Video", TRIM_NONE},
  {"", TRIM_ALL, "", TRIM_NONE},
  {"  ", TRIM_LEADING, "", TRIM_LEADING},
  {"  ", TRIM_TRAILING, "", TRIM_TRAILING},
  {"  ", TRIM_ALL, "", TRIM_ALL},
  {"\t\rTest String\n", TRIM_ALL, "Test String", TRIM_ALL},
};

TEST(StringUtilTest, TrimWhitespace) {
  std::wstring output;  // Allow contents to carry over to next testcase
  for (size_t i = 0; i < arraysize(trim_cases); ++i) {
    const trim_case& value = trim_cases[i];
    EXPECT_EQ(value.return_value,
              TrimWhitespace(value.input, value.positions, &output));
    EXPECT_EQ(value.output, output);
  }

  // Test that TrimWhitespace() can take the same string for input and output
  output = L"  This is a test \r\n";
  EXPECT_EQ(TRIM_ALL, TrimWhitespace(output, TRIM_ALL, &output));
  EXPECT_EQ(L"This is a test", output);

  // Once more, but with a string of whitespace
  output = L"  \r\n";
  EXPECT_EQ(TRIM_ALL, TrimWhitespace(output, TRIM_ALL, &output));
  EXPECT_EQ(L"", output);

  std::string output_ascii;
  for (size_t i = 0; i < arraysize(trim_cases_ascii); ++i) {
    const trim_case_ascii& value = trim_cases_ascii[i];
    EXPECT_EQ(value.return_value,
              TrimWhitespace(value.input, value.positions, &output_ascii));
    EXPECT_EQ(value.output, output_ascii);
  }
}

static const struct collapse_case {
  const wchar_t* input;
  const bool trim;
  const wchar_t* output;
} collapse_cases[] = {
  {L" Google Video ", false, L"Google Video"},
  {L"Google Video", false, L"Google Video"},
  {L"", false, L""},
  {L"  ", false, L""},
  {L"\t\rTest String\n", false, L"Test String"},
  {L"\x2002Test String\x00A0\x3000", false, L"Test String"},
  {L"    Test     \n  \t String    ", false, L"Test String"},
  {L"\x2002Test\x1680 \x2028 \tString\x00A0\x3000", false, L"Test String"},
  {L"   Test String", false, L"Test String"},
  {L"Test String    ", false, L"Test String"},
  {L"Test String", false, L"Test String"},
  {L"", true, L""},
  {L"\n", true, L""},
  {L"  \r  ", true, L""},
  {L"\nFoo", true, L"Foo"},
  {L"\r  Foo  ", true, L"Foo"},
  {L" Foo bar ", true, L"Foo bar"},
  {L"  \tFoo  bar  \n", true, L"Foo bar"},
  {L" a \r b\n c \r\n d \t\re \t f \n ", true, L"abcde f"},
};

TEST(StringUtilTest, CollapseWhitespace) {
  for (size_t i = 0; i < arraysize(collapse_cases); ++i) {
    const collapse_case& value = collapse_cases[i];
    EXPECT_EQ(value.output, CollapseWhitespace(value.input, value.trim));
  }
}

static const struct collapse_case_ascii {
  const char* input;
  const bool trim;
  const char* output;
} collapse_cases_ascii[] = {
  {" Google Video ", false, "Google Video"},
  {"Google Video", false, "Google Video"},
  {"", false, ""},
  {"  ", false, ""},
  {"\t\rTest String\n", false, "Test String"},
  {"    Test     \n  \t String    ", false, "Test String"},
  {"   Test String", false, "Test String"},
  {"Test String    ", false, "Test String"},
  {"Test String", false, "Test String"},
  {"", true, ""},
  {"\n", true, ""},
  {"  \r  ", true, ""},
  {"\nFoo", true, "Foo"},
  {"\r  Foo  ", true, "Foo"},
  {" Foo bar ", true, "Foo bar"},
  {"  \tFoo  bar  \n", true, "Foo bar"},
  {" a \r b\n c \r\n d \t\re \t f \n ", true, "abcde f"},
};

TEST(StringUtilTest, CollapseWhitespaceASCII) {
  for (size_t i = 0; i < arraysize(collapse_cases_ascii); ++i) {
    const collapse_case_ascii& value = collapse_cases_ascii[i];
    EXPECT_EQ(value.output, CollapseWhitespaceASCII(value.input, value.trim));
  }
}

TEST(StringUtilTest, ContainsOnlyWhitespaceASCII) {
  EXPECT_TRUE(ContainsOnlyWhitespaceASCII(""));
  EXPECT_TRUE(ContainsOnlyWhitespaceASCII(" "));
  EXPECT_TRUE(ContainsOnlyWhitespaceASCII("\t"));
  EXPECT_TRUE(ContainsOnlyWhitespaceASCII("\t \r \n  "));
  EXPECT_FALSE(ContainsOnlyWhitespaceASCII("a"));
  EXPECT_FALSE(ContainsOnlyWhitespaceASCII("\thello\r \n  "));
}

TEST(StringUtilTest, ContainsOnlyWhitespace) {
  EXPECT_TRUE(ContainsOnlyWhitespace(ASCIIToUTF16("")));
  EXPECT_TRUE(ContainsOnlyWhitespace(ASCIIToUTF16(" ")));
  EXPECT_TRUE(ContainsOnlyWhitespace(ASCIIToUTF16("\t")));
  EXPECT_TRUE(ContainsOnlyWhitespace(ASCIIToUTF16("\t \r \n  ")));
  EXPECT_FALSE(ContainsOnlyWhitespace(ASCIIToUTF16("a")));
  EXPECT_FALSE(ContainsOnlyWhitespace(ASCIIToUTF16("\thello\r \n  ")));
}

TEST(StringUtilTest, IsStringUTF8) {
  EXPECT_TRUE(IsStringUTF8("abc"));
  EXPECT_TRUE(IsStringUTF8("\xc2\x81"));
  EXPECT_TRUE(IsStringUTF8("\xe1\x80\xbf"));
  EXPECT_TRUE(IsStringUTF8("\xf1\x80\xa0\xbf"));
  EXPECT_TRUE(IsStringUTF8("a\xc2\x81\xe1\x80\xbf\xf1\x80\xa0\xbf"));
  EXPECT_TRUE(IsStringUTF8("\xef\xbb\xbf" "abc"));  // UTF-8 BOM

  // surrogate code points
  EXPECT_FALSE(IsStringUTF8("\xed\xa0\x80\xed\xbf\xbf"));
  EXPECT_FALSE(IsStringUTF8("\xed\xa0\x8f"));
  EXPECT_FALSE(IsStringUTF8("\xed\xbf\xbf"));

  // overlong sequences
  EXPECT_FALSE(IsStringUTF8("\xc0\x80"));  // U+0000
  EXPECT_FALSE(IsStringUTF8("\xc1\x80\xc1\x81"));  // "AB"
  EXPECT_FALSE(IsStringUTF8("\xe0\x80\x80"));  // U+0000
  EXPECT_FALSE(IsStringUTF8("\xe0\x82\x80"));  // U+0080
  EXPECT_FALSE(IsStringUTF8("\xe0\x9f\xbf"));  // U+07ff
  EXPECT_FALSE(IsStringUTF8("\xf0\x80\x80\x8D"));  // U+000D
  EXPECT_FALSE(IsStringUTF8("\xf0\x80\x82\x91"));  // U+0091
  EXPECT_FALSE(IsStringUTF8("\xf0\x80\xa0\x80"));  // U+0800
  EXPECT_FALSE(IsStringUTF8("\xf0\x8f\xbb\xbf"));  // U+FEFF (BOM)
  EXPECT_FALSE(IsStringUTF8("\xf8\x80\x80\x80\xbf"));  // U+003F
  EXPECT_FALSE(IsStringUTF8("\xfc\x80\x80\x80\xa0\xa5"));  // U+00A5

  // Beyond U+10FFFF (the upper limit of Unicode codespace)
  EXPECT_FALSE(IsStringUTF8("\xf4\x90\x80\x80"));  // U+110000
  EXPECT_FALSE(IsStringUTF8("\xf8\xa0\xbf\x80\xbf"));  // 5 bytes
  EXPECT_FALSE(IsStringUTF8("\xfc\x9c\xbf\x80\xbf\x80"));  // 6 bytes

  // BOMs in UTF-16(BE|LE) and UTF-32(BE|LE)
  EXPECT_FALSE(IsStringUTF8("\xfe\xff"));
  EXPECT_FALSE(IsStringUTF8("\xff\xfe"));
  EXPECT_FALSE(IsStringUTF8(std::string("\x00\x00\xfe\xff", 4)));
  EXPECT_FALSE(IsStringUTF8("\xff\xfe\x00\x00"));

  // Non-characters : U+xxFFF[EF] where xx is 0x00 through 0x10 and <FDD0,FDEF>
  EXPECT_FALSE(IsStringUTF8("\xef\xbf\xbe"));  // U+FFFE)
  EXPECT_FALSE(IsStringUTF8("\xf0\x8f\xbf\xbe"));  // U+1FFFE
  EXPECT_FALSE(IsStringUTF8("\xf3\xbf\xbf\xbf"));  // U+10FFFF

  // This should also be false, but currently we pass them through.
  // Disable them for now.
#if 0
  EXPECT_FALSE(IsStringUTF8("\xef\xb7\x90"));  // U+FDD0
  EXPECT_FALSE(IsStringUTF8("\xef\xb7\xaf"));  // U+FDEF
#endif

  // Strings in legacy encodings. We can certainly make up strings
  // in a legacy encoding that are valid in UTF-8, but in real data,
  // most of them are invalid as UTF-8.
  EXPECT_FALSE(IsStringUTF8("caf\xe9"));  // cafe with U+00E9 in ISO-8859-1
  EXPECT_FALSE(IsStringUTF8("\xb0\xa1\xb0\xa2"));  // U+AC00, U+AC001 in EUC-KR
  EXPECT_FALSE(IsStringUTF8("\xa7\x41\xa6\x6e"));  // U+4F60 U+597D in Big5
  // "abc" with U+201[CD] in windows-125[0-8]
  EXPECT_FALSE(IsStringUTF8("\x93" "abc\x94"));
  // U+0639 U+064E U+0644 U+064E in ISO-8859-6
  EXPECT_FALSE(IsStringUTF8("\xd9\xee\xe4\xee"));
  // U+03B3 U+03B5 U+03B9 U+03AC in ISO-8859-7
  EXPECT_FALSE(IsStringUTF8("\xe3\xe5\xe9\xdC"));
}

TEST(StringUtilTest, ConvertASCII) {
  static const char* char_cases[] = {
    "Google Video",
    "Hello, world\n",
    "0123ABCDwxyz \a\b\t\r\n!+,.~"
  };

  static const wchar_t* const wchar_cases[] = {
    L"Google Video",
    L"Hello, world\n",
    L"0123ABCDwxyz \a\b\t\r\n!+,.~"
  };

  for (size_t i = 0; i < arraysize(char_cases); ++i) {
    EXPECT_TRUE(IsStringASCII(char_cases[i]));
    std::wstring wide = ASCIIToWide(char_cases[i]);
    EXPECT_EQ(wchar_cases[i], wide);

    EXPECT_TRUE(IsStringASCII(wchar_cases[i]));
    std::string ascii = WideToASCII(wchar_cases[i]);
    EXPECT_EQ(char_cases[i], ascii);
  }

  EXPECT_FALSE(IsStringASCII("Google \x80Video"));
  EXPECT_FALSE(IsStringASCII(L"Google \x80Video"));

  // Convert empty strings.
  std::wstring wempty;
  std::string empty;
  EXPECT_EQ(empty, WideToASCII(wempty));
  EXPECT_EQ(wempty, ASCIIToWide(empty));

  // Convert strings with an embedded NUL character.
  const char chars_with_nul[] = "test\0string";
  const int length_with_nul = arraysize(chars_with_nul) - 1;
  std::string string_with_nul(chars_with_nul, length_with_nul);
  std::wstring wide_with_nul = ASCIIToWide(string_with_nul);
  EXPECT_EQ(static_cast<std::wstring::size_type>(length_with_nul),
            wide_with_nul.length());
  std::string narrow_with_nul = WideToASCII(wide_with_nul);
  EXPECT_EQ(static_cast<std::string::size_type>(length_with_nul),
            narrow_with_nul.length());
  EXPECT_EQ(0, string_with_nul.compare(narrow_with_nul));
}

TEST(StringUtilTest, ToUpperASCII) {
  EXPECT_EQ('C', ToUpperASCII('C'));
  EXPECT_EQ('C', ToUpperASCII('c'));
  EXPECT_EQ('2', ToUpperASCII('2'));

  EXPECT_EQ(L'C', ToUpperASCII(L'C'));
  EXPECT_EQ(L'C', ToUpperASCII(L'c'));
  EXPECT_EQ(L'2', ToUpperASCII(L'2'));

  std::string in_place_a("Cc2");
  StringToUpperASCII(&in_place_a);
  EXPECT_EQ("CC2", in_place_a);

  std::wstring in_place_w(L"Cc2");
  StringToUpperASCII(&in_place_w);
  EXPECT_EQ(L"CC2", in_place_w);

  std::string original_a("Cc2");
  std::string upper_a = StringToUpperASCII(original_a);
  EXPECT_EQ("CC2", upper_a);

  std::wstring original_w(L"Cc2");
  std::wstring upper_w = StringToUpperASCII(original_w);
  EXPECT_EQ(L"CC2", upper_w);
}

static const struct {
  const wchar_t* src_w;
  const char*    src_a;
  const char*    dst;
} lowercase_cases[] = {
  {L"FoO", "FoO", "foo"},
  {L"foo", "foo", "foo"},
  {L"FOO", "FOO", "foo"},
};

TEST(StringUtilTest, LowerCaseEqualsASCII) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(lowercase_cases); ++i) {
    EXPECT_TRUE(LowerCaseEqualsASCII(lowercase_cases[i].src_w,
                                     lowercase_cases[i].dst));
    EXPECT_TRUE(LowerCaseEqualsASCII(lowercase_cases[i].src_a,
                                     lowercase_cases[i].dst));
  }
}

TEST(StringUtilTest, GetByteDisplayUnits) {
  static const struct {
    int64 bytes;
    DataUnits expected;
  } cases[] = {
    {0, DATA_UNITS_BYTE},
    {512, DATA_UNITS_BYTE},
    {10*1024, DATA_UNITS_KIBIBYTE},
    {10*1024*1024, DATA_UNITS_MEBIBYTE},
    {10LL*1024*1024*1024, DATA_UNITS_GIBIBYTE},
    {~(1LL<<63), DATA_UNITS_GIBIBYTE},
#ifdef NDEBUG
    {-1, DATA_UNITS_BYTE},
#endif
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i)
    EXPECT_EQ(cases[i].expected, GetByteDisplayUnits(cases[i].bytes));
}

TEST(StringUtilTest, FormatBytes) {
  static const struct {
    int64 bytes;
    DataUnits units;
    const wchar_t* expected;
    const wchar_t* expected_with_units;
  } cases[] = {
    {0, DATA_UNITS_BYTE, L"0", L"0 B"},
    {512, DATA_UNITS_BYTE, L"512", L"512 B"},
    {512, DATA_UNITS_KIBIBYTE, L"0.5", L"0.5 kB"},
    {1024*1024, DATA_UNITS_KIBIBYTE, L"1024", L"1024 kB"},
    {1024*1024, DATA_UNITS_MEBIBYTE, L"1", L"1 MB"},
    {1024*1024*1024, DATA_UNITS_GIBIBYTE, L"1", L"1 GB"},
    {10LL*1024*1024*1024, DATA_UNITS_GIBIBYTE, L"10", L"10 GB"},
    {~(1LL<<63), DATA_UNITS_GIBIBYTE, L"8589934592", L"8589934592 GB"},
    // Make sure the first digit of the fractional part works.
    {1024*1024 + 103, DATA_UNITS_KIBIBYTE, L"1024.1", L"1024.1 kB"},
    {1024*1024 + 205 * 1024, DATA_UNITS_MEBIBYTE, L"1.2", L"1.2 MB"},
    {1024*1024*1024 + (927 * 1024*1024), DATA_UNITS_GIBIBYTE,
     L"1.9", L"1.9 GB"},
    {10LL*1024*1024*1024, DATA_UNITS_GIBIBYTE, L"10", L"10 GB"},
#ifdef NDEBUG
    {-1, DATA_UNITS_BYTE, L"", L""},
#endif
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    EXPECT_EQ(cases[i].expected,
              FormatBytes(cases[i].bytes, cases[i].units, false));
    EXPECT_EQ(cases[i].expected_with_units,
              FormatBytes(cases[i].bytes, cases[i].units, true));
  }
}

TEST(StringUtilTest, ReplaceSubstringsAfterOffset) {
  static const struct {
    const char* str;
    string16::size_type start_offset;
    const char* find_this;
    const char* replace_with;
    const char* expected;
  } cases[] = {
    {"aaa", 0, "a", "b", "bbb"},
    {"abb", 0, "ab", "a", "ab"},
    {"Removing some substrings inging", 0, "ing", "", "Remov some substrs "},
    {"Not found", 0, "x", "0", "Not found"},
    {"Not found again", 5, "x", "0", "Not found again"},
    {" Making it much longer ", 0, " ", "Four score and seven years ago",
     "Four score and seven years agoMakingFour score and seven years agoit"
     "Four score and seven years agomuchFour score and seven years agolonger"
     "Four score and seven years ago"},
    {"Invalid offset", 9999, "t", "foobar", "Invalid offset"},
    {"Replace me only me once", 9, "me ", "", "Replace me only once"},
    {"abababab", 2, "ab", "c", "abccc"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    string16 str = ASCIIToUTF16(cases[i].str);
    ReplaceSubstringsAfterOffset(&str, cases[i].start_offset,
                                 ASCIIToUTF16(cases[i].find_this),
                                 ASCIIToUTF16(cases[i].replace_with));
    EXPECT_EQ(ASCIIToUTF16(cases[i].expected), str);
  }
}

TEST(StringUtilTest, ReplaceFirstSubstringAfterOffset) {
  static const struct {
    const char* str;
    string16::size_type start_offset;
    const char* find_this;
    const char* replace_with;
    const char* expected;
  } cases[] = {
    {"aaa", 0, "a", "b", "baa"},
    {"abb", 0, "ab", "a", "ab"},
    {"Removing some substrings inging", 0, "ing", "",
      "Remov some substrings inging"},
    {"Not found", 0, "x", "0", "Not found"},
    {"Not found again", 5, "x", "0", "Not found again"},
    {" Making it much longer ", 0, " ", "Four score and seven years ago",
     "Four score and seven years agoMaking it much longer "},
    {"Invalid offset", 9999, "t", "foobar", "Invalid offset"},
    {"Replace me only me once", 4, "me ", "", "Replace only me once"},
    {"abababab", 2, "ab", "c", "abcabab"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); i++) {
    string16 str = ASCIIToUTF16(cases[i].str);
    ReplaceFirstSubstringAfterOffset(&str, cases[i].start_offset,
                                     ASCIIToUTF16(cases[i].find_this),
                                     ASCIIToUTF16(cases[i].replace_with));
    EXPECT_EQ(ASCIIToUTF16(cases[i].expected), str);
  }
}

namespace {

template <typename INT>
struct IntToStringTest {
  INT num;
  const char* sexpected;
  const char* uexpected;
};

}

TEST(StringUtilTest, IntToString) {
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
    EXPECT_EQ(IntToWString(test->num), UTF8ToWide(test->sexpected));
    EXPECT_EQ(UintToString(test->num), test->uexpected);
    EXPECT_EQ(UintToWString(test->num), UTF8ToWide(test->uexpected));
  }
  for (size_t i = 0; i < arraysize(int64_tests); ++i) {
    const IntToStringTest<int64>* test = &int64_tests[i];
    EXPECT_EQ(Int64ToString(test->num), test->sexpected);
    EXPECT_EQ(Int64ToWString(test->num), UTF8ToWide(test->sexpected));
    EXPECT_EQ(Uint64ToString(test->num), test->uexpected);
    EXPECT_EQ(Uint64ToWString(test->num), UTF8ToWide(test->uexpected));
  }
}

TEST(StringUtilTest, Uint64ToString) {
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

TEST(StringUtilTest, StringToInt) {
  static const struct {
    std::string input;
    int output;
    bool success;
  } cases[] = {
    {"0", 0, true},
    {"42", 42, true},
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
    EXPECT_EQ(cases[i].output, StringToInt(cases[i].input));
    int output;
    EXPECT_EQ(cases[i].success, StringToInt(cases[i].input, &output));
    EXPECT_EQ(cases[i].output, output);

    std::wstring wide_input = ASCIIToWide(cases[i].input);
    EXPECT_EQ(cases[i].output, StringToInt(WideToUTF16Hack(wide_input)));
    EXPECT_EQ(cases[i].success, StringToInt(WideToUTF16Hack(wide_input),
                                            &output));
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

  std::wstring wide_input = ASCIIToWide(input_string);
  EXPECT_FALSE(StringToInt(WideToUTF16Hack(wide_input), &output));
  EXPECT_EQ(6, output);
}

TEST(StringUtilTest, StringToInt64) {
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
    EXPECT_EQ(cases[i].output, StringToInt64(cases[i].input));
    int64 output;
    EXPECT_EQ(cases[i].success, StringToInt64(cases[i].input, &output));
    EXPECT_EQ(cases[i].output, output);

    std::wstring wide_input = ASCIIToWide(cases[i].input);
    EXPECT_EQ(cases[i].output, StringToInt64(WideToUTF16Hack(wide_input)));
    EXPECT_EQ(cases[i].success, StringToInt64(WideToUTF16Hack(wide_input),
                                              &output));
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

  std::wstring wide_input = ASCIIToWide(input_string);
  EXPECT_FALSE(StringToInt64(WideToUTF16Hack(wide_input), &output));
  EXPECT_EQ(6, output);
}

TEST(StringUtilTest, HexStringToInt) {
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
    {"efgh", 0xef, false},
    {"0xefgh", 0xef, false},
    {"hgfe", 0, false},
    {"100000000", -1, false},  // don't care about |output|, just |success|
    {"-", 0, false},
    {"", 0, false},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    EXPECT_EQ(cases[i].output, HexStringToInt(cases[i].input));
    int output;
    EXPECT_EQ(cases[i].success, HexStringToInt(cases[i].input, &output));
    EXPECT_EQ(cases[i].output, output);

    std::wstring wide_input = ASCIIToWide(cases[i].input);
    EXPECT_EQ(cases[i].output, HexStringToInt(WideToUTF16Hack(wide_input)));
    EXPECT_EQ(cases[i].success, HexStringToInt(WideToUTF16Hack(wide_input),
                                               &output));
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

  std::wstring wide_input = ASCIIToWide(input_string);
  EXPECT_FALSE(HexStringToInt(WideToUTF16Hack(wide_input), &output));
  EXPECT_EQ(0xc0ffee, output);
}

TEST(StringUtilTest, HexStringToBytes) {
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

    output.clear();
    compare.clear();

    std::wstring wide_input = ASCIIToWide(cases[i].input);
    EXPECT_EQ(cases[i].success,
              HexStringToBytes(WideToUTF16Hack(wide_input), &output)) <<
        i << ": " << cases[i].input;
    for (size_t j = 0; j < cases[i].output_len; ++j)
      compare.push_back(static_cast<uint8>(cases[i].output[j]));
    ASSERT_EQ(output.size(), compare.size()) << i << ": " << cases[i].input;
    EXPECT_TRUE(std::equal(output.begin(), output.end(), compare.begin())) <<
        i << ": " << cases[i].input;
  }
}

TEST(StringUtilTest, StringToDouble) {
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
    EXPECT_DOUBLE_EQ(cases[i].output, StringToDouble(cases[i].input));
    double output;
    EXPECT_EQ(cases[i].success, StringToDouble(cases[i].input, &output));
    EXPECT_DOUBLE_EQ(cases[i].output, output);

    std::wstring wide_input = ASCIIToWide(cases[i].input);
    EXPECT_DOUBLE_EQ(cases[i].output,
                     StringToDouble(WideToUTF16Hack(wide_input)));
    EXPECT_EQ(cases[i].success, StringToDouble(WideToUTF16Hack(wide_input),
                                               &output));
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

  std::wstring wide_input = ASCIIToWide(input_string);
  EXPECT_FALSE(StringToDouble(WideToUTF16Hack(wide_input), &output));
  EXPECT_DOUBLE_EQ(3.14, output);
}

// This checks where we can use the assignment operator for a va_list. We need
// a way to do this since Visual C doesn't support va_copy, but assignment on
// va_list is not guaranteed to be a copy. See StringAppendVT which uses this
// capability.
static void VariableArgsFunc(const char* format, ...) {
  va_list org;
  va_start(org, format);

  va_list dup;
  GG_VA_COPY(dup, org);
  int i1 = va_arg(org, int);
  int j1 = va_arg(org, int);
  char* s1 = va_arg(org, char*);
  double d1 = va_arg(org, double);
  va_end(org);

  int i2 = va_arg(dup, int);
  int j2 = va_arg(dup, int);
  char* s2 = va_arg(dup, char*);
  double d2 = va_arg(dup, double);

  EXPECT_EQ(i1, i2);
  EXPECT_EQ(j1, j2);
  EXPECT_STREQ(s1, s2);
  EXPECT_EQ(d1, d2);

  va_end(dup);
}

TEST(StringUtilTest, VAList) {
  VariableArgsFunc("%d %d %s %lf", 45, 92, "This is interesting", 9.21);
}

TEST(StringUtilTest, StringPrintfEmpty) {
  EXPECT_EQ("", StringPrintf("%s", ""));
}

TEST(StringUtilTest, StringPrintfMisc) {
  EXPECT_EQ("123hello w", StringPrintf("%3d%2s %1c", 123, "hello", 'w'));
  EXPECT_EQ(L"123hello w", StringPrintf(L"%3d%2ls %1lc", 123, L"hello", 'w'));
}

TEST(StringUtilTest, StringAppendfEmptyString) {
  std::string value("Hello");
  StringAppendF(&value, "%s", "");
  EXPECT_EQ("Hello", value);

  std::wstring valuew(L"Hello");
  StringAppendF(&valuew, L"%ls", L"");
  EXPECT_EQ(L"Hello", valuew);
}

TEST(StringUtilTest, StringAppendfString) {
  std::string value("Hello");
  StringAppendF(&value, " %s", "World");
  EXPECT_EQ("Hello World", value);

  std::wstring valuew(L"Hello");
  StringAppendF(&valuew, L" %ls", L"World");
  EXPECT_EQ(L"Hello World", valuew);
}

TEST(StringUtilTest, StringAppendfInt) {
  std::string value("Hello");
  StringAppendF(&value, " %d", 123);
  EXPECT_EQ("Hello 123", value);

  std::wstring valuew(L"Hello");
  StringAppendF(&valuew, L" %d", 123);
  EXPECT_EQ(L"Hello 123", valuew);
}

// Make sure that lengths exactly around the initial buffer size are handled
// correctly.
TEST(StringUtilTest, StringPrintfBounds) {
  const int kSrcLen = 1026;
  char src[kSrcLen];
  for (size_t i = 0; i < arraysize(src); i++)
    src[i] = 'A';

  wchar_t srcw[kSrcLen];
  for (size_t i = 0; i < arraysize(srcw); i++)
    srcw[i] = 'A';

  for (int i = 1; i < 3; i++) {
    src[kSrcLen - i] = 0;
    std::string out;
    SStringPrintf(&out, "%s", src);
    EXPECT_STREQ(src, out.c_str());

    srcw[kSrcLen - i] = 0;
    std::wstring outw;
    SStringPrintf(&outw, L"%ls", srcw);
    EXPECT_STREQ(srcw, outw.c_str());
  }
}

// Test very large sprintfs that will cause the buffer to grow.
TEST(StringUtilTest, Grow) {
  char src[1026];
  for (size_t i = 0; i < arraysize(src); i++)
    src[i] = 'A';
  src[1025] = 0;

  const char* fmt = "%sB%sB%sB%sB%sB%sB%s";

  std::string out;
  SStringPrintf(&out, fmt, src, src, src, src, src, src, src);

  const int kRefSize = 320000;
  char* ref = new char[kRefSize];
#if defined(OS_WIN)
  sprintf_s(ref, kRefSize, fmt, src, src, src, src, src, src, src);
#elif defined(OS_POSIX)
  snprintf(ref, kRefSize, fmt, src, src, src, src, src, src, src);
#endif

  EXPECT_STREQ(ref, out.c_str());
  delete[] ref;
}

// A helper for the StringAppendV test that follows.
// Just forwards its args to StringAppendV.
static void StringAppendVTestHelper(std::string* out,
                                    const char* format,
                                    ...) PRINTF_FORMAT(2, 3);

static void StringAppendVTestHelper(std::string* out, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  StringAppendV(out, format, ap);
  va_end(ap);
}

TEST(StringUtilTest, StringAppendV) {
  std::string out;
  StringAppendVTestHelper(&out, "%d foo %s", 1, "bar");
  EXPECT_EQ("1 foo bar", out);
}

// Test the boundary condition for the size of the string_util's
// internal buffer.
TEST(StringUtilTest, GrowBoundary) {
  const int string_util_buf_len = 1024;
  // Our buffer should be one larger than the size of StringAppendVT's stack
  // buffer.
  const int buf_len = string_util_buf_len + 1;
  char src[buf_len + 1];  // Need extra one for NULL-terminator.
  for (int i = 0; i < buf_len; ++i)
    src[i] = 'a';
  src[buf_len] = 0;

  std::string out;
  SStringPrintf(&out, "%s", src);

  EXPECT_STREQ(src, out.c_str());
}

// TODO(evanm): what's the proper cross-platform test here?
#if defined(OS_WIN)
// sprintf in Visual Studio fails when given U+FFFF. This tests that the
// failure case is gracefuly handled.
TEST(StringUtilTest, Invalid) {
  wchar_t invalid[2];
  invalid[0] = 0xffff;
  invalid[1] = 0;

  std::wstring out;
  SStringPrintf(&out, L"%ls", invalid);
  EXPECT_STREQ(L"", out.c_str());
}
#endif

// Test for SplitString
TEST(StringUtilTest, SplitString) {
  std::vector<std::wstring> r;

  SplitString(L"a,b,c", L',', &r);
  ASSERT_EQ(3U, r.size());
  EXPECT_EQ(r[0], L"a");
  EXPECT_EQ(r[1], L"b");
  EXPECT_EQ(r[2], L"c");
  r.clear();

  SplitString(L"a, b, c", L',', &r);
  ASSERT_EQ(3U, r.size());
  EXPECT_EQ(r[0], L"a");
  EXPECT_EQ(r[1], L"b");
  EXPECT_EQ(r[2], L"c");
  r.clear();

  SplitString(L"a,,c", L',', &r);
  ASSERT_EQ(3U, r.size());
  EXPECT_EQ(r[0], L"a");
  EXPECT_EQ(r[1], L"");
  EXPECT_EQ(r[2], L"c");
  r.clear();

  SplitString(L"", L'*', &r);
  ASSERT_EQ(1U, r.size());
  EXPECT_EQ(r[0], L"");
  r.clear();

  SplitString(L"foo", L'*', &r);
  ASSERT_EQ(1U, r.size());
  EXPECT_EQ(r[0], L"foo");
  r.clear();

  SplitString(L"foo ,", L',', &r);
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], L"foo");
  EXPECT_EQ(r[1], L"");
  r.clear();

  SplitString(L",", L',', &r);
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], L"");
  EXPECT_EQ(r[1], L"");
  r.clear();

  SplitString(L"\t\ta\t", L'\t', &r);
  ASSERT_EQ(4U, r.size());
  EXPECT_EQ(r[0], L"");
  EXPECT_EQ(r[1], L"");
  EXPECT_EQ(r[2], L"a");
  EXPECT_EQ(r[3], L"");
  r.clear();

  SplitStringDontTrim(L"\t\ta\t", L'\t', &r);
  ASSERT_EQ(4U, r.size());
  EXPECT_EQ(r[0], L"");
  EXPECT_EQ(r[1], L"");
  EXPECT_EQ(r[2], L"a");
  EXPECT_EQ(r[3], L"");
  r.clear();

  SplitString(L"\ta\t\nb\tcc", L'\n', &r);
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], L"a");
  EXPECT_EQ(r[1], L"b\tcc");
  r.clear();

  SplitStringDontTrim(L"\ta\t\nb\tcc", L'\n', &r);
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], L"\ta\t");
  EXPECT_EQ(r[1], L"b\tcc");
  r.clear();
}

// Test for Tokenize
TEST(StringUtilTest, Tokenize) {
  std::vector<std::string> r;
  size_t size;

  size = Tokenize("This is a string", " ", &r);
  EXPECT_EQ(4U, size);
  ASSERT_EQ(4U, r.size());
  EXPECT_EQ(r[0], "This");
  EXPECT_EQ(r[1], "is");
  EXPECT_EQ(r[2], "a");
  EXPECT_EQ(r[3], "string");
  r.clear();

  size = Tokenize("one,two,three", ",", &r);
  EXPECT_EQ(3U, size);
  ASSERT_EQ(3U, r.size());
  EXPECT_EQ(r[0], "one");
  EXPECT_EQ(r[1], "two");
  EXPECT_EQ(r[2], "three");
  r.clear();

  size = Tokenize("one,two:three;four", ",:", &r);
  EXPECT_EQ(3U, size);
  ASSERT_EQ(3U, r.size());
  EXPECT_EQ(r[0], "one");
  EXPECT_EQ(r[1], "two");
  EXPECT_EQ(r[2], "three;four");
  r.clear();

  size = Tokenize("one,two:three;four", ";,:", &r);
  EXPECT_EQ(4U, size);
  ASSERT_EQ(4U, r.size());
  EXPECT_EQ(r[0], "one");
  EXPECT_EQ(r[1], "two");
  EXPECT_EQ(r[2], "three");
  EXPECT_EQ(r[3], "four");
  r.clear();

  size = Tokenize("one, two, three", ",", &r);
  EXPECT_EQ(3U, size);
  ASSERT_EQ(3U, r.size());
  EXPECT_EQ(r[0], "one");
  EXPECT_EQ(r[1], " two");
  EXPECT_EQ(r[2], " three");
  r.clear();

  size = Tokenize("one, two, three, ", ",", &r);
  EXPECT_EQ(4U, size);
  ASSERT_EQ(4U, r.size());
  EXPECT_EQ(r[0], "one");
  EXPECT_EQ(r[1], " two");
  EXPECT_EQ(r[2], " three");
  EXPECT_EQ(r[3], " ");
  r.clear();

  size = Tokenize("one, two, three,", ",", &r);
  EXPECT_EQ(3U, size);
  ASSERT_EQ(3U, r.size());
  EXPECT_EQ(r[0], "one");
  EXPECT_EQ(r[1], " two");
  EXPECT_EQ(r[2], " three");
  r.clear();

  size = Tokenize("", ",", &r);
  EXPECT_EQ(0U, size);
  ASSERT_EQ(0U, r.size());
  r.clear();

  size = Tokenize(",", ",", &r);
  EXPECT_EQ(0U, size);
  ASSERT_EQ(0U, r.size());
  r.clear();

  size = Tokenize(",;:.", ".:;,", &r);
  EXPECT_EQ(0U, size);
  ASSERT_EQ(0U, r.size());
  r.clear();

  size = Tokenize("\t\ta\t", "\t", &r);
  EXPECT_EQ(1U, size);
  ASSERT_EQ(1U, r.size());
  EXPECT_EQ(r[0], "a");
  r.clear();

  size = Tokenize("\ta\t\nb\tcc", "\n", &r);
  EXPECT_EQ(2U, size);
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], "\ta\t");
  EXPECT_EQ(r[1], "b\tcc");
  r.clear();
}

// Test for JoinString
TEST(StringUtilTest, JoinString) {
  std::vector<std::string> in;
  EXPECT_EQ("", JoinString(in, ','));

  in.push_back("a");
  EXPECT_EQ("a", JoinString(in, ','));

  in.push_back("b");
  in.push_back("c");
  EXPECT_EQ("a,b,c", JoinString(in, ','));

  in.push_back("");
  EXPECT_EQ("a,b,c,", JoinString(in, ','));
  in.push_back(" ");
  EXPECT_EQ("a|b|c|| ", JoinString(in, '|'));
}

TEST(StringUtilTest, StartsWith) {
  EXPECT_TRUE(StartsWithASCII("javascript:url", "javascript", true));
  EXPECT_FALSE(StartsWithASCII("JavaScript:url", "javascript", true));
  EXPECT_TRUE(StartsWithASCII("javascript:url", "javascript", false));
  EXPECT_TRUE(StartsWithASCII("JavaScript:url", "javascript", false));
  EXPECT_FALSE(StartsWithASCII("java", "javascript", true));
  EXPECT_FALSE(StartsWithASCII("java", "javascript", false));
  EXPECT_FALSE(StartsWithASCII("", "javascript", false));
  EXPECT_FALSE(StartsWithASCII("", "javascript", true));
  EXPECT_TRUE(StartsWithASCII("java", "", false));
  EXPECT_TRUE(StartsWithASCII("java", "", true));

  EXPECT_TRUE(StartsWith(L"javascript:url", L"javascript", true));
  EXPECT_FALSE(StartsWith(L"JavaScript:url", L"javascript", true));
  EXPECT_TRUE(StartsWith(L"javascript:url", L"javascript", false));
  EXPECT_TRUE(StartsWith(L"JavaScript:url", L"javascript", false));
  EXPECT_FALSE(StartsWith(L"java", L"javascript", true));
  EXPECT_FALSE(StartsWith(L"java", L"javascript", false));
  EXPECT_FALSE(StartsWith(L"", L"javascript", false));
  EXPECT_FALSE(StartsWith(L"", L"javascript", true));
  EXPECT_TRUE(StartsWith(L"java", L"", false));
  EXPECT_TRUE(StartsWith(L"java", L"", true));
}

TEST(StringUtilTest, EndsWith) {
  EXPECT_TRUE(EndsWith(L"Foo.plugin", L".plugin", true));
  EXPECT_FALSE(EndsWith(L"Foo.Plugin", L".plugin", true));
  EXPECT_TRUE(EndsWith(L"Foo.plugin", L".plugin", false));
  EXPECT_TRUE(EndsWith(L"Foo.Plugin", L".plugin", false));
  EXPECT_FALSE(EndsWith(L".plug", L".plugin", true));
  EXPECT_FALSE(EndsWith(L".plug", L".plugin", false));
  EXPECT_FALSE(EndsWith(L"Foo.plugin Bar", L".plugin", true));
  EXPECT_FALSE(EndsWith(L"Foo.plugin Bar", L".plugin", false));
  EXPECT_FALSE(EndsWith(L"", L".plugin", false));
  EXPECT_FALSE(EndsWith(L"", L".plugin", true));
  EXPECT_TRUE(EndsWith(L"Foo.plugin", L"", false));
  EXPECT_TRUE(EndsWith(L"Foo.plugin", L"", true));
  EXPECT_TRUE(EndsWith(L".plugin", L".plugin", false));
  EXPECT_TRUE(EndsWith(L".plugin", L".plugin", true));
  EXPECT_TRUE(EndsWith(L"", L"", false));
  EXPECT_TRUE(EndsWith(L"", L"", true));
}

TEST(StringUtilTest, GetStringFWithOffsets) {
  std::vector<string16> subst;
  subst.push_back(ASCIIToUTF16("1"));
  subst.push_back(ASCIIToUTF16("2"));
  std::vector<size_t> offsets;

  ReplaceStringPlaceholders(ASCIIToUTF16("Hello, $1. Your number is $2."),
                            subst,
                            &offsets);
  EXPECT_EQ(2U, offsets.size());
  EXPECT_EQ(7U, offsets[0]);
  EXPECT_EQ(25U, offsets[1]);
  offsets.clear();

  ReplaceStringPlaceholders(ASCIIToUTF16("Hello, $2. Your number is $1."),
                            subst,
                            &offsets);
  EXPECT_EQ(2U, offsets.size());
  EXPECT_EQ(25U, offsets[0]);
  EXPECT_EQ(7U, offsets[1]);
  offsets.clear();
}

TEST(StringUtilTest, ReplaceStringPlaceholders) {
  std::vector<string16> subst;
  subst.push_back(ASCIIToUTF16("9a"));
  subst.push_back(ASCIIToUTF16("8b"));
  subst.push_back(ASCIIToUTF16("7c"));
  subst.push_back(ASCIIToUTF16("6d"));
  subst.push_back(ASCIIToUTF16("5e"));
  subst.push_back(ASCIIToUTF16("4f"));
  subst.push_back(ASCIIToUTF16("3g"));
  subst.push_back(ASCIIToUTF16("2h"));
  subst.push_back(ASCIIToUTF16("1i"));

  string16 formatted =
      ReplaceStringPlaceholders(
          ASCIIToUTF16("$1a,$2b,$3c,$4d,$5e,$6f,$7g,$8h,$9i"), subst, NULL);

  EXPECT_EQ(formatted, ASCIIToUTF16("9aa,8bb,7cc,6dd,5ee,4ff,3gg,2hh,1ii"));
}

TEST(StringUtilTest, ReplaceStringPlaceholdersTooFew) {
  // Test whether replacestringplaceholders works as expected when there
  // are fewer inputs than outputs.
  std::vector<string16> subst;
  subst.push_back(ASCIIToUTF16("9a"));
  subst.push_back(ASCIIToUTF16("8b"));
  subst.push_back(ASCIIToUTF16("7c"));

  string16 formatted =
      ReplaceStringPlaceholders(
          ASCIIToUTF16("$1a,$2b,$3c,$4d,$5e,$6f,$1g,$2h,$3i"), subst, NULL);

  EXPECT_EQ(formatted, ASCIIToUTF16("9aa,8bb,7cc,d,e,f,9ag,8bh,7ci"));
}

TEST(StringUtilTest, StdStringReplaceStringPlaceholders) {
  std::vector<std::string> subst;
  subst.push_back("9a");
  subst.push_back("8b");
  subst.push_back("7c");
  subst.push_back("6d");
  subst.push_back("5e");
  subst.push_back("4f");
  subst.push_back("3g");
  subst.push_back("2h");
  subst.push_back("1i");

  std::string formatted =
      ReplaceStringPlaceholders(
          "$1a,$2b,$3c,$4d,$5e,$6f,$7g,$8h,$9i", subst, NULL);

  EXPECT_EQ(formatted, "9aa,8bb,7cc,6dd,5ee,4ff,3gg,2hh,1ii");
}

TEST(StringUtilTest, SplitStringAlongWhitespace) {
  struct TestData {
    const std::wstring input;
    const size_t expected_result_count;
    const std::wstring output1;
    const std::wstring output2;
  } data[] = {
    { L"a",       1, L"a",  L""   },
    { L" ",       0, L"",   L""   },
    { L" a",      1, L"a",  L""   },
    { L" ab ",    1, L"ab", L""   },
    { L" ab c",   2, L"ab", L"c"  },
    { L" ab c ",  2, L"ab", L"c"  },
    { L" ab cd",  2, L"ab", L"cd" },
    { L" ab cd ", 2, L"ab", L"cd" },
    { L" \ta\t",  1, L"a",  L""   },
    { L" b\ta\t", 2, L"b",  L"a"  },
    { L" b\tat",  2, L"b",  L"at" },
    { L"b\tat",   2, L"b",  L"at" },
    { L"b\t at",  2, L"b",  L"at" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    std::vector<std::wstring> results;
    SplitStringAlongWhitespace(data[i].input, &results);
    ASSERT_EQ(data[i].expected_result_count, results.size());
    if (data[i].expected_result_count > 0)
      ASSERT_EQ(data[i].output1, results[0]);
    if (data[i].expected_result_count > 1)
      ASSERT_EQ(data[i].output2, results[1]);
  }
}

TEST(StringUtilTest, MatchPatternTest) {
  EXPECT_EQ(MatchPatternASCII("www.google.com", "*.com"), true);
  EXPECT_EQ(MatchPatternASCII("www.google.com", "*"), true);
  EXPECT_EQ(MatchPatternASCII("www.google.com", "www*.g*.org"), false);
  EXPECT_EQ(MatchPatternASCII("Hello", "H?l?o"), true);
  EXPECT_EQ(MatchPatternASCII("www.google.com", "http://*)"), false);
  EXPECT_EQ(MatchPatternASCII("www.msn.com", "*.COM"), false);
  EXPECT_EQ(MatchPatternASCII("Hello*1234", "He??o\\*1*"), true);
  EXPECT_EQ(MatchPatternASCII("", "*.*"), false);
  EXPECT_EQ(MatchPatternASCII("", "*"), true);
  EXPECT_EQ(MatchPatternASCII("", "?"), true);
  EXPECT_EQ(MatchPatternASCII("", ""), true);
  EXPECT_EQ(MatchPatternASCII("Hello", ""), false);
  EXPECT_EQ(MatchPatternASCII("Hello*", "Hello*"), true);
  // Stop after a certain recursion depth.
  EXPECT_EQ(MatchPatternASCII("12345678901234567890", "???????????????????*"),
                              false);
}

TEST(StringUtilTest, LcpyTest) {
  // Test the normal case where we fit in our buffer.
  {
    char dst[10];
    wchar_t wdst[10];
    EXPECT_EQ(7U, base::strlcpy(dst, "abcdefg", arraysize(dst)));
    EXPECT_EQ(0, memcmp(dst, "abcdefg", 8));
    EXPECT_EQ(7U, base::wcslcpy(wdst, L"abcdefg", arraysize(wdst)));
    EXPECT_EQ(0, memcmp(wdst, L"abcdefg", sizeof(wchar_t) * 8));
  }

  // Test dst_size == 0, nothing should be written to |dst| and we should
  // have the equivalent of strlen(src).
  {
    char dst[2] = {1, 2};
    wchar_t wdst[2] = {1, 2};
    EXPECT_EQ(7U, base::strlcpy(dst, "abcdefg", 0));
    EXPECT_EQ(1, dst[0]);
    EXPECT_EQ(2, dst[1]);
    EXPECT_EQ(7U, base::wcslcpy(wdst, L"abcdefg", 0));
#if defined(WCHAR_T_IS_UNSIGNED)
    EXPECT_EQ(1U, wdst[0]);
    EXPECT_EQ(2U, wdst[1]);
#else
    EXPECT_EQ(1, wdst[0]);
    EXPECT_EQ(2, wdst[1]);
#endif
  }

  // Test the case were we _just_ competely fit including the null.
  {
    char dst[8];
    wchar_t wdst[8];
    EXPECT_EQ(7U, base::strlcpy(dst, "abcdefg", arraysize(dst)));
    EXPECT_EQ(0, memcmp(dst, "abcdefg", 8));
    EXPECT_EQ(7U, base::wcslcpy(wdst, L"abcdefg", arraysize(wdst)));
    EXPECT_EQ(0, memcmp(wdst, L"abcdefg", sizeof(wchar_t) * 8));
  }

  // Test the case were we we are one smaller, so we can't fit the null.
  {
    char dst[7];
    wchar_t wdst[7];
    EXPECT_EQ(7U, base::strlcpy(dst, "abcdefg", arraysize(dst)));
    EXPECT_EQ(0, memcmp(dst, "abcdef", 7));
    EXPECT_EQ(7U, base::wcslcpy(wdst, L"abcdefg", arraysize(wdst)));
    EXPECT_EQ(0, memcmp(wdst, L"abcdef", sizeof(wchar_t) * 7));
  }

  // Test the case were we are just too small.
  {
    char dst[3];
    wchar_t wdst[3];
    EXPECT_EQ(7U, base::strlcpy(dst, "abcdefg", arraysize(dst)));
    EXPECT_EQ(0, memcmp(dst, "ab", 3));
    EXPECT_EQ(7U, base::wcslcpy(wdst, L"abcdefg", arraysize(wdst)));
    EXPECT_EQ(0, memcmp(wdst, L"ab", sizeof(wchar_t) * 3));
  }
}

TEST(StringUtilTest, WprintfFormatPortabilityTest) {
  struct TestData {
    const wchar_t* input;
    bool portable;
  } cases[] = {
    { L"%ls", true },
    { L"%s", false },
    { L"%S", false },
    { L"%lS", false },
    { L"Hello, %s", false },
    { L"%lc", true },
    { L"%c", false },
    { L"%C", false },
    { L"%lC", false },
    { L"%ls %s", false },
    { L"%s %ls", false },
    { L"%s %ls %s", false },
    { L"%f", true },
    { L"%f %F", false },
    { L"%d %D", false },
    { L"%o %O", false },
    { L"%u %U", false },
    { L"%f %d %o %u", true },
    { L"%-8d (%02.1f%)", true },
    { L"% 10s", false },
    { L"% 10ls", true }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    EXPECT_EQ(cases[i].portable, base::IsWprintfFormatPortable(cases[i].input));
  }
}

TEST(StringUtilTest, ElideString) {
  struct TestData {
    const wchar_t* input;
    int max_len;
    bool result;
    const wchar_t* output;
  } cases[] = {
    { L"Hello", 0, true, L"" },
    { L"", 0, false, L"" },
    { L"Hello, my name is Tom", 1, true, L"H" },
    { L"Hello, my name is Tom", 2, true, L"He" },
    { L"Hello, my name is Tom", 3, true, L"H.m" },
    { L"Hello, my name is Tom", 4, true, L"H..m" },
    { L"Hello, my name is Tom", 5, true, L"H...m" },
    { L"Hello, my name is Tom", 6, true, L"He...m" },
    { L"Hello, my name is Tom", 7, true, L"He...om" },
    { L"Hello, my name is Tom", 10, true, L"Hell...Tom" },
    { L"Hello, my name is Tom", 100, false, L"Hello, my name is Tom" }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(cases); ++i) {
    std::wstring output;
    EXPECT_EQ(cases[i].result,
              ElideString(cases[i].input, cases[i].max_len, &output));
    EXPECT_TRUE(output == cases[i].output);
  }
}

TEST(StringUtilTest, HexEncode) {
  std::string hex(HexEncode(NULL, 0));
  EXPECT_EQ(hex.length(), 0U);
  unsigned char bytes[] = {0x01, 0xff, 0x02, 0xfe, 0x03, 0x80, 0x81};
  hex = HexEncode(bytes, sizeof(bytes));
  EXPECT_EQ(hex.compare("01FF02FE038081"), 0);
}

}  // namaspace base

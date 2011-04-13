// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "net/base/escape.h"

#include "base/basictypes.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static const size_t kNpos = string16::npos;

struct EscapeCase {
  const wchar_t* input;
  const wchar_t* output;
};

struct UnescapeURLCase {
  const wchar_t* input;
  UnescapeRule::Type rules;
  const wchar_t* output;
};

struct UnescapeURLCaseASCII {
  const char* input;
  UnescapeRule::Type rules;
  const char* output;
};

struct UnescapeAndDecodeCase {
  const char* input;

  // The expected output when run through UnescapeURL.
  const char* url_unescaped;

  // The expected output when run through UnescapeQuery.
  const char* query_unescaped;

  // The expected output when run through UnescapeAndDecodeURLComponent.
  const wchar_t* decoded;
};

struct AdjustOffsetCase {
  const char* input;
  size_t input_offset;
  size_t output_offset;
};

struct EscapeForHTMLCase {
  const char* input;
  const char* expected_output;
};

}  // namespace

TEST(EscapeTest, EscapeTextForFormSubmission) {
  const EscapeCase escape_cases[] = {
    {L"foo", L"foo"},
    {L"foo bar", L"foo+bar"},
    {L"foo++", L"foo%2B%2B"}
  };
  for (size_t i = 0; i < arraysize(escape_cases); ++i) {
    EscapeCase value = escape_cases[i];
    EXPECT_EQ(WideToUTF16Hack(value.output),
              EscapeQueryParamValueUTF8(WideToUTF16Hack(value.input), true));
  }

  const EscapeCase escape_cases_no_plus[] = {
    {L"foo", L"foo"},
    {L"foo bar", L"foo%20bar"},
    {L"foo++", L"foo%2B%2B"}
  };
  for (size_t i = 0; i < arraysize(escape_cases_no_plus); ++i) {
    EscapeCase value = escape_cases_no_plus[i];
    EXPECT_EQ(WideToUTF16Hack(value.output),
              EscapeQueryParamValueUTF8(WideToUTF16Hack(value.input), false));
  }

  // Test all the values in we're supposed to be escaping.
  const std::string no_escape(
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "!'()*-._~");
  for (int i = 0; i < 256; ++i) {
    std::string in;
    in.push_back(i);
    std::string out = EscapeQueryParamValue(in, true);
    if (0 == i) {
      EXPECT_EQ(out, std::string("%00"));
    } else if (32 == i) {
      // Spaces are plus escaped like web forms.
      EXPECT_EQ(out, std::string("+"));
    } else if (no_escape.find(in) == std::string::npos) {
      // Check %hex escaping
      std::string expected = base::StringPrintf("%%%02X", i);
      EXPECT_EQ(expected, out);
    } else {
      // No change for things in the no_escape list.
      EXPECT_EQ(out, in);
    }
  }

  // Check to see if EscapeQueryParamValueUTF8 is the same as
  // EscapeQueryParamValue(..., kCodepageUTF8,)
  string16 test_str;
  test_str.reserve(5000);
  for (int i = 1; i < 5000; ++i) {
    test_str.push_back(i);
  }
  string16 wide;
  EXPECT_TRUE(EscapeQueryParamValue(test_str, base::kCodepageUTF8, true,
                                    &wide));
  EXPECT_EQ(wide, EscapeQueryParamValueUTF8(test_str, true));
  EXPECT_TRUE(EscapeQueryParamValue(test_str, base::kCodepageUTF8, false,
                                    &wide));
  EXPECT_EQ(wide, EscapeQueryParamValueUTF8(test_str, false));
}

TEST(EscapeTest, EscapePath) {
  ASSERT_EQ(
    // Most of the character space we care about, un-escaped
    EscapePath(
      "\x02\n\x1d !\"#$%&'()*+,-./0123456789:;"
      "<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "[\\]^_`abcdefghijklmnopqrstuvwxyz"
      "{|}~\x7f\x80\xff"),
    // Escaped
    "%02%0A%1D%20!%22%23$%25&'()*+,-./0123456789%3A;"
    "%3C=%3E%3F@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "%5B%5C%5D%5E_%60abcdefghijklmnopqrstuvwxyz"
    "%7B%7C%7D~%7F%80%FF");
}

TEST(EscapeTest, EscapeUrlEncodedData) {
  ASSERT_EQ(
    // Most of the character space we care about, un-escaped
    EscapeUrlEncodedData(
      "\x02\n\x1d !\"#$%&'()*+,-./0123456789:;"
      "<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "[\\]^_`abcdefghijklmnopqrstuvwxyz"
      "{|}~\x7f\x80\xff"),
    // Escaped
    "%02%0A%1D+!%22%23%24%25%26%27()*%2B,-./0123456789:%3B"
    "%3C%3D%3E%3F%40ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "%5B%5C%5D%5E_%60abcdefghijklmnopqrstuvwxyz"
    "%7B%7C%7D~%7F%80%FF");
}

TEST(EscapeTest, UnescapeURLComponentASCII) {
  const UnescapeURLCaseASCII unescape_cases[] = {
    {"", UnescapeRule::NORMAL, ""},
    {"%2", UnescapeRule::NORMAL, "%2"},
    {"%%%%%%", UnescapeRule::NORMAL, "%%%%%%"},
    {"Don't escape anything", UnescapeRule::NORMAL, "Don't escape anything"},
    {"Invalid %escape %2", UnescapeRule::NORMAL, "Invalid %escape %2"},
    {"Some%20random text %25%2dOK", UnescapeRule::NONE,
     "Some%20random text %25%2dOK"},
    {"Some%20random text %25%2dOK", UnescapeRule::NORMAL,
     "Some%20random text %25-OK"},
    {"Some%20random text %25%2dOK", UnescapeRule::SPACES,
     "Some random text %25-OK"},
    {"Some%20random text %25%2dOK", UnescapeRule::URL_SPECIAL_CHARS,
     "Some%20random text %-OK"},
    {"Some%20random text %25%2dOK",
     UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS,
     "Some random text %-OK"},
    {"%A0%B1%C2%D3%E4%F5", UnescapeRule::NORMAL, "\xA0\xB1\xC2\xD3\xE4\xF5"},
    {"%Aa%Bb%Cc%Dd%Ee%Ff", UnescapeRule::NORMAL, "\xAa\xBb\xCc\xDd\xEe\xFf"},
    // Certain URL-sensitive characters should not be unescaped unless asked.
    {"Hello%20%13%10world %23# %3F? %3D= %26& %25% %2B+", UnescapeRule::SPACES,
     "Hello %13%10world %23# %3F? %3D= %26& %25% %2B+"},
    {"Hello%20%13%10world %23# %3F? %3D= %26& %25% %2B+",
     UnescapeRule::URL_SPECIAL_CHARS,
     "Hello%20%13%10world ## ?? == && %% ++"},
    // Control characters.
    {"%01%02%03%04%05%06%07%08%09 %25", UnescapeRule::URL_SPECIAL_CHARS,
     "%01%02%03%04%05%06%07%08%09 %"},
    {"%01%02%03%04%05%06%07%08%09 %25", UnescapeRule::CONTROL_CHARS,
     "\x01\x02\x03\x04\x05\x06\x07\x08\x09 %25"},
    {"Hello%20%13%10%02", UnescapeRule::SPACES, "Hello %13%10%02"},
    {"Hello%20%13%10%02", UnescapeRule::CONTROL_CHARS, "Hello%20\x13\x10\x02"},
  };

  for (size_t i = 0; i < arraysize(unescape_cases); i++) {
    std::string str(unescape_cases[i].input);
    EXPECT_EQ(std::string(unescape_cases[i].output),
              UnescapeURLComponent(str, unescape_cases[i].rules));
  }

  // Test the NULL character unescaping (which wouldn't work above since those
  // are just char pointers).
  std::string input("Null");
  input.push_back(0);  // Also have a NULL in the input.
  input.append("%00%39Test");

  // When we're unescaping NULLs
  std::string expected("Null");
  expected.push_back(0);
  expected.push_back(0);
  expected.append("9Test");
  EXPECT_EQ(expected, UnescapeURLComponent(input, UnescapeRule::CONTROL_CHARS));

  // When we're not unescaping NULLs.
  expected = "Null";
  expected.push_back(0);
  expected.append("%009Test");
  EXPECT_EQ(expected, UnescapeURLComponent(input, UnescapeRule::NORMAL));
}

TEST(EscapeTest, UnescapeURLComponent) {
  const UnescapeURLCase unescape_cases[] = {
    {L"", UnescapeRule::NORMAL, L""},
    {L"%2", UnescapeRule::NORMAL, L"%2"},
    {L"%%%%%%", UnescapeRule::NORMAL, L"%%%%%%"},
    {L"Don't escape anything", UnescapeRule::NORMAL, L"Don't escape anything"},
    {L"Invalid %escape %2", UnescapeRule::NORMAL, L"Invalid %escape %2"},
    {L"Some%20random text %25%2dOK", UnescapeRule::NONE,
     L"Some%20random text %25%2dOK"},
    {L"Some%20random text %25%2dOK", UnescapeRule::NORMAL,
     L"Some%20random text %25-OK"},
    {L"Some%20random text %25%2dOK", UnescapeRule::SPACES,
     L"Some random text %25-OK"},
    {L"Some%20random text %25%2dOK", UnescapeRule::URL_SPECIAL_CHARS,
     L"Some%20random text %-OK"},
    {L"Some%20random text %25%2dOK",
     UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS,
     L"Some random text %-OK"},
    {L"%A0%B1%C2%D3%E4%F5", UnescapeRule::NORMAL, L"\xA0\xB1\xC2\xD3\xE4\xF5"},
    {L"%Aa%Bb%Cc%Dd%Ee%Ff", UnescapeRule::NORMAL, L"\xAa\xBb\xCc\xDd\xEe\xFf"},
    // Certain URL-sensitive characters should not be unescaped unless asked.
    {L"Hello%20%13%10world %23# %3F? %3D= %26& %25% %2B+", UnescapeRule::SPACES,
     L"Hello %13%10world %23# %3F? %3D= %26& %25% %2B+"},
    {L"Hello%20%13%10world %23# %3F? %3D= %26& %25% %2B+",
     UnescapeRule::URL_SPECIAL_CHARS,
     L"Hello%20%13%10world ## ?? == && %% ++"},
    // We can neither escape nor unescape '@' since some websites expect it to
    // be preserved as either '@' or "%40".
    // See http://b/996720 and http://crbug.com/23933 .
    {L"me@my%40example", UnescapeRule::NORMAL, L"me@my%40example"},
    // Control characters.
    {L"%01%02%03%04%05%06%07%08%09 %25", UnescapeRule::URL_SPECIAL_CHARS,
     L"%01%02%03%04%05%06%07%08%09 %"},
    {L"%01%02%03%04%05%06%07%08%09 %25", UnescapeRule::CONTROL_CHARS,
     L"\x01\x02\x03\x04\x05\x06\x07\x08\x09 %25"},
    {L"Hello%20%13%10%02", UnescapeRule::SPACES, L"Hello %13%10%02"},
    {L"Hello%20%13%10%02", UnescapeRule::CONTROL_CHARS,
     L"Hello%20\x13\x10\x02"},
    {L"Hello\x9824\x9827", UnescapeRule::CONTROL_CHARS,
     L"Hello\x9824\x9827"},
  };

  for (size_t i = 0; i < arraysize(unescape_cases); i++) {
    string16 str(WideToUTF16(unescape_cases[i].input));
    EXPECT_EQ(WideToUTF16(unescape_cases[i].output),
              UnescapeURLComponent(str, unescape_cases[i].rules));
  }

  // Test the NULL character unescaping (which wouldn't work above since those
  // are just char pointers).
  string16 input(WideToUTF16(L"Null"));
  input.push_back(0);  // Also have a NULL in the input.
  input.append(WideToUTF16(L"%00%39Test"));

  // When we're unescaping NULLs
  string16 expected(WideToUTF16(L"Null"));
  expected.push_back(0);
  expected.push_back(0);
  expected.append(ASCIIToUTF16("9Test"));
  EXPECT_EQ(expected, UnescapeURLComponent(input, UnescapeRule::CONTROL_CHARS));

  // When we're not unescaping NULLs.
  expected = WideToUTF16(L"Null");
  expected.push_back(0);
  expected.append(WideToUTF16(L"%009Test"));
  EXPECT_EQ(expected, UnescapeURLComponent(input, UnescapeRule::NORMAL));
}

TEST(EscapeTest, UnescapeAndDecodeUTF8URLComponent) {
  const UnescapeAndDecodeCase unescape_cases[] = {
    { "%",
      "%",
      "%",
     L"%"},
    { "+",
      "+",
      " ",
     L"+"},
    { "%2+",
      "%2+",
      "%2 ",
     L"%2+"},
    { "+%%%+%%%",
      "+%%%+%%%",
      " %%% %%%",
     L"+%%%+%%%"},
    { "Don't escape anything",
      "Don't escape anything",
      "Don't escape anything",
     L"Don't escape anything"},
    { "+Invalid %escape %2+",
      "+Invalid %escape %2+",
      " Invalid %escape %2 ",
     L"+Invalid %escape %2+"},
    { "Some random text %25%2dOK",
      "Some random text %25-OK",
      "Some random text %25-OK",
     L"Some random text %25-OK"},
    { "%01%02%03%04%05%06%07%08%09",
      "%01%02%03%04%05%06%07%08%09",
      "%01%02%03%04%05%06%07%08%09",
     L"%01%02%03%04%05%06%07%08%09"},
    { "%E4%BD%A0+%E5%A5%BD",
      "\xE4\xBD\xA0+\xE5\xA5\xBD",
      "\xE4\xBD\xA0 \xE5\xA5\xBD",
     L"\x4f60+\x597d"},
    { "%ED%ED",  // Invalid UTF-8.
      "\xED\xED",
      "\xED\xED",
     L"%ED%ED"},  // Invalid UTF-8 -> kept unescaped.
  };

  for (size_t i = 0; i < arraysize(unescape_cases); i++) {
    std::string unescaped = UnescapeURLComponent(unescape_cases[i].input,
                                                 UnescapeRule::NORMAL);
    EXPECT_EQ(std::string(unescape_cases[i].url_unescaped), unescaped);

    unescaped = UnescapeURLComponent(unescape_cases[i].input,
                                     UnescapeRule::REPLACE_PLUS_WITH_SPACE);
    EXPECT_EQ(std::string(unescape_cases[i].query_unescaped), unescaped);

    // TODO: Need to test unescape_spaces and unescape_percent.
    string16 decoded = UnescapeAndDecodeUTF8URLComponent(
        unescape_cases[i].input, UnescapeRule::NORMAL, NULL);
    EXPECT_EQ(WideToUTF16Hack(std::wstring(unescape_cases[i].decoded)),
              decoded);
  }
}

TEST(EscapeTest, AdjustOffset) {
  const AdjustOffsetCase adjust_cases[] = {
    {"", 0, std::wstring::npos},
    {"test", 0, 0},
    {"test", 2, 2},
    {"test", 4, std::wstring::npos},
    {"test", std::wstring::npos, std::wstring::npos},
    {"%2dtest", 6, 4},
    {"%2dtest", 2, std::wstring::npos},
    {"test%2d", 2, 2},
    {"%E4%BD%A0+%E5%A5%BD", 9, 1},
    {"%E4%BD%A0+%E5%A5%BD", 6, std::wstring::npos},
    {"%ED%B0%80+%E5%A5%BD", 6, 6},
  };

  for (size_t i = 0; i < arraysize(adjust_cases); i++) {
    size_t offset = adjust_cases[i].input_offset;
    UnescapeAndDecodeUTF8URLComponent(adjust_cases[i].input,
                                      UnescapeRule::NORMAL, &offset);
    EXPECT_EQ(adjust_cases[i].output_offset, offset);
  }
}

TEST(EscapeTest, EscapeForHTML) {
  const EscapeForHTMLCase tests[] = {
    { "hello", "hello" },
    { "<hello>", "&lt;hello&gt;" },
    { "don\'t mess with me", "don&#39;t mess with me" },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string result = EscapeForHTML(std::string(tests[i].input));
    EXPECT_EQ(std::string(tests[i].expected_output), result);
  }
}

TEST(EscapeTest, UnescapeForHTML) {
  const EscapeForHTMLCase tests[] = {
    { "", "" },
    { "&lt;hello&gt;", "<hello>" },
    { "don&#39;t mess with me", "don\'t mess with me" },
    { "&lt;&gt;&amp;&quot;&#39;", "<>&\"'" },
    { "& lt; &amp ; &; '", "& lt; &amp ; &; '" },
    { "&amp;", "&" },
    { "&quot;", "\"" },
    { "&#39;", "'" },
    { "&lt;", "<" },
    { "&gt;", ">" },
    { "&amp; &", "& &" },
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    string16 result = UnescapeForHTML(ASCIIToUTF16(tests[i].input));
    EXPECT_EQ(ASCIIToUTF16(tests[i].expected_output), result);
  }
}

TEST(EscapeTest, AdjustEncodingOffset) {
  // Imagine we have strings as shown in the following cases where the
  // %XX's represent encoded characters

  // 1: abc%ECdef ==> abcXdef
  std::vector<size_t> offsets;
  for (size_t t = 0; t < 9; ++t)
    offsets.push_back(t);
  AdjustEncodingOffset::Adjustments adjustments;
  adjustments.push_back(3);
  std::for_each(offsets.begin(), offsets.end(),
                AdjustEncodingOffset(adjustments));
  size_t expected_1[] = {0, 1, 2, 3, kNpos, kNpos, 4, 5, 6};
  EXPECT_EQ(offsets.size(), arraysize(expected_1));
  for (size_t i = 0; i < arraysize(expected_1); ++i)
    EXPECT_EQ(expected_1[i], offsets[i]);


  // 2: %ECabc%EC%ECdef%EC ==> XabcXXdefX
  offsets.clear();
  for (size_t t = 0; t < 18; ++t)
    offsets.push_back(t);
  adjustments.clear();
  adjustments.push_back(0);
  adjustments.push_back(6);
  adjustments.push_back(9);
  adjustments.push_back(15);
  std::for_each(offsets.begin(), offsets.end(),
                AdjustEncodingOffset(adjustments));
  size_t expected_2[] = {0, kNpos, kNpos, 1, 2, 3, 4, kNpos, kNpos, 5, kNpos,
                         kNpos, 6, 7, 8, 9, kNpos, kNpos};
  EXPECT_EQ(offsets.size(), arraysize(expected_2));
  for (size_t i = 0; i < arraysize(expected_2); ++i)
    EXPECT_EQ(expected_2[i], offsets[i]);
}

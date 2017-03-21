// Copyright 2007, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <errno.h>
#include <unicode/ucnv.h>

#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_canon_icu.h"
#include "googleurl/src/url_canon_internal.h"
#include "googleurl/src/url_canon_stdstring.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/url_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

// Some implementations of base/basictypes.h may define ARRAYSIZE.
// If it's not defined, we define it to the ARRAYSIZE_UNSAFE macro
// which is in our version of basictypes.h.
#ifndef ARRAYSIZE
#define ARRAYSIZE ARRAYSIZE_UNSAFE
#endif

using url_test_utils::WStringToUTF16;
using url_test_utils::ConvertUTF8ToUTF16;
using url_test_utils::ConvertUTF16ToUTF8;
using url_canon::CanonHostInfo;

namespace {

struct ComponentCase {
  const char* input;
  const char* expected;
  url_parse::Component expected_component;
  bool expected_success;
};

// ComponentCase but with dual 8-bit/16-bit input. Generally, the unit tests
// treat each input as optional, and will only try processing if non-NULL.
// The output is always 8-bit.
struct DualComponentCase {
  const char* input8;
  const wchar_t* input16;
  const char* expected;
  url_parse::Component expected_component;
  bool expected_success;
};

// Test cases for CanonicalizeIPAddress().  The inputs are identical to
// DualComponentCase, but the output has extra CanonHostInfo fields.
struct IPAddressCase {
  const char* input8;
  const wchar_t* input16;
  const char* expected;
  url_parse::Component expected_component;

  // CanonHostInfo fields, for verbose output.
  CanonHostInfo::Family expected_family;
  int expected_num_ipv4_components;
  const char* expected_address_hex;  // Two hex chars per IP address byte.
};

std::string BytesToHexString(unsigned char bytes[16], int length) {
  EXPECT_TRUE(length == 0 || length == 4 || length == 16)
      << "Bad IP address length: " << length;
  std::string result;
  for (int i = 0; i < length; ++i) {
    result.push_back(url_canon::kHexCharLookup[(bytes[i] >> 4) & 0xf]);
    result.push_back(url_canon::kHexCharLookup[bytes[i] & 0xf]);
  }
  return result;
}

struct ReplaceCase {
  const char* base;
  const char* scheme;
  const char* username;
  const char* password;
  const char* host;
  const char* port;
  const char* path;
  const char* query;
  const char* ref;
  const char* expected;
};

// Wrapper around a UConverter object that managers creation and destruction.
class UConvScoper {
 public:
  explicit UConvScoper(const char* charset_name) {
    UErrorCode err = U_ZERO_ERROR;
    converter_ = ucnv_open(charset_name, &err);
  }

  ~UConvScoper() {
    if (converter_)
      ucnv_close(converter_);
  }

  // Returns the converter object, may be NULL.
  UConverter* converter() const { return converter_; }

 private:
  UConverter* converter_;
};

// Magic string used in the replacements code that tells SetupReplComp to
// call the clear function.
const char kDeleteComp[] = "|";

// Sets up a replacement for a single component. This is given pointers to
// the set and clear function for the component being replaced, and will
// either set the component (if it exists) or clear it (if the replacement
// string matches kDeleteComp).
//
// This template is currently used only for the 8-bit case, and the strlen
// causes it to fail in other cases. It is left a template in case we have
// tests for wide replacements.
template<typename CHAR>
void SetupReplComp(
    void (url_canon::Replacements<CHAR>::*set)(const CHAR*,
                                               const url_parse::Component&),
    void (url_canon::Replacements<CHAR>::*clear)(),
    url_canon::Replacements<CHAR>* rep,
    const CHAR* str) {
  if (str && str[0] == kDeleteComp[0]) {
    (rep->*clear)();
  } else if (str) {
    (rep->*set)(str, url_parse::Component(0, static_cast<int>(strlen(str))));
  }
}

}  // namespace

TEST(URLCanonTest, DoAppendUTF8) {
  struct UTF8Case {
    unsigned input;
    const char* output;
  } utf_cases[] = {
    // Valid code points.
    {0x24, "\x24"},
    {0xA2, "\xC2\xA2"},
    {0x20AC, "\xE2\x82\xAC"},
    {0x24B62, "\xF0\xA4\xAD\xA2"},
    {0x10FFFF, "\xF4\x8F\xBF\xBF"},
  };
  std::string out_str;
  for (size_t i = 0; i < ARRAYSIZE(utf_cases); i++) {
    out_str.clear();
    url_canon::StdStringCanonOutput output(&out_str);
    url_canon::AppendUTF8Value(utf_cases[i].input, &output);
    output.Complete();
    EXPECT_EQ(utf_cases[i].output, out_str);
  }
}

// TODO(mattm): Can't run this in debug mode for now, since the DCHECK will
// cause the Chromium stacktrace dialog to appear and hang the test.
// See http://crbug.com/49580.
#if defined(GTEST_HAS_DEATH_TEST) && defined(NDEBUG)
TEST(URLCanonTest, DoAppendUTF8Invalid) {
  std::string out_str;
  url_canon::StdStringCanonOutput output(&out_str);
  // Invalid code point (too large).
  ASSERT_DEBUG_DEATH({
    url_canon::AppendUTF8Value(0x110000, &output);
    output.Complete();
    EXPECT_EQ("", out_str);
  }, "");
}
#endif

TEST(URLCanonTest, UTF) {
  // Low-level test that we handle reading, canonicalization, and writing
  // UTF-8/UTF-16 strings properly.
  struct UTFCase {
    const char* input8;
    const wchar_t* input16;
    bool expected_success;
    const char* output;
  } utf_cases[] = {
      // Valid canonical input should get passed through & escaped.
    {"\xe4\xbd\xa0\xe5\xa5\xbd", L"\x4f60\x597d", true, "%E4%BD%A0%E5%A5%BD"},
      // Test a characer that takes > 16 bits (U+10300 = old italic letter A)
    {"\xF0\x90\x8C\x80", L"\xd800\xdf00", true, "%F0%90%8C%80"},
      // Non-shortest-form UTF-8 are invalid. The bad char should be replaced
      // with the invalid character (EF BF DB in UTF-8).
    {"\xf0\x84\xbd\xa0\xe5\xa5\xbd", NULL, false, "%EF%BF%BD%E5%A5%BD"},
      // Invalid UTF-8 sequences should be marked as invalid (the first
      // sequence is truncated).
    {"\xe4\xa0\xe5\xa5\xbd", L"\xd800\x597d", false, "%EF%BF%BD%E5%A5%BD"},
      // Character going off the end.
    {"\xe4\xbd\xa0\xe5\xa5", L"\x4f60\xd800", false, "%E4%BD%A0%EF%BF%BD"},
      // ...same with low surrogates with no high surrogate.
    {"\xed\xb0\x80", L"\xdc00", false, "%EF%BF%BD"},
      // Test a UTF-8 encoded surrogate value is marked as invalid.
      // ED A0 80 = U+D800
    {"\xed\xa0\x80", NULL, false, "%EF%BF%BD"},
  };

  std::string out_str;
  for (size_t i = 0; i < ARRAYSIZE(utf_cases); i++) {
    if (utf_cases[i].input8) {
      out_str.clear();
      url_canon::StdStringCanonOutput output(&out_str);

      int input_len = static_cast<int>(strlen(utf_cases[i].input8));
      bool success = true;
      for (int ch = 0; ch < input_len; ch++) {
        success &= AppendUTF8EscapedChar(utf_cases[i].input8, &ch, input_len,
                                         &output);
      }
      output.Complete();
      EXPECT_EQ(utf_cases[i].expected_success, success);
      EXPECT_EQ(std::string(utf_cases[i].output), out_str);
    }
    if (utf_cases[i].input16) {
      out_str.clear();
      url_canon::StdStringCanonOutput output(&out_str);

      string16 input_str(WStringToUTF16(utf_cases[i].input16));
      int input_len = static_cast<int>(input_str.length());
      bool success = true;
      for (int ch = 0; ch < input_len; ch++) {
        success &= AppendUTF8EscapedChar(input_str.c_str(), &ch, input_len,
                                         &output);
      }
      output.Complete();
      EXPECT_EQ(utf_cases[i].expected_success, success);
      EXPECT_EQ(std::string(utf_cases[i].output), out_str);
    }

    if (utf_cases[i].input8 && utf_cases[i].input16 &&
        utf_cases[i].expected_success) {
      // Check that the UTF-8 and UTF-16 inputs are equivalent.

      // UTF-16 -> UTF-8
      std::string input8_str(utf_cases[i].input8);
      string16 input16_str(WStringToUTF16(utf_cases[i].input16));
      EXPECT_EQ(input8_str, ConvertUTF16ToUTF8(input16_str));

      // UTF-8 -> UTF-16
      EXPECT_EQ(input16_str, ConvertUTF8ToUTF16(input8_str));
    }
  }
}

TEST(URLCanonTest, ICUCharsetConverter) {
  struct ICUCase {
    const wchar_t* input;
    const char* encoding;
    const char* expected;
  } icu_cases[] = {
      // UTF-8.
    {L"Hello, world", "utf-8", "Hello, world"},
    {L"\x4f60\x597d", "utf-8", "\xe4\xbd\xa0\xe5\xa5\xbd"},
      // Non-BMP UTF-8.
    {L"!\xd800\xdf00!", "utf-8", "!\xf0\x90\x8c\x80!"},
      // Big5
    {L"\x4f60\x597d", "big5", "\xa7\x41\xa6\x6e"},
      // Unrepresentable character in the destination set.
    {L"hello\x4f60\x06de\x597dworld", "big5", "hello\xa7\x41%26%231758%3B\xa6\x6eworld"},
  };

  for (size_t i = 0; i < ARRAYSIZE(icu_cases); i++) {
    UConvScoper conv(icu_cases[i].encoding);
    ASSERT_TRUE(conv.converter() != NULL);
    url_canon::ICUCharsetConverter converter(conv.converter());

    std::string str;
    url_canon::StdStringCanonOutput output(&str);

    string16 input_str(WStringToUTF16(icu_cases[i].input));
    int input_len = static_cast<int>(input_str.length());
    converter.ConvertFromUTF16(input_str.c_str(), input_len, &output);
    output.Complete();

    EXPECT_STREQ(icu_cases[i].expected, str.c_str());
  }

  // Test string sizes around the resize boundary for the output to make sure
  // the converter resizes as needed.
  const int static_size = 16;
  UConvScoper conv("utf-8");
  ASSERT_TRUE(conv.converter());
  url_canon::ICUCharsetConverter converter(conv.converter());
  for (int i = static_size - 2; i <= static_size + 2; i++) {
    // Make a string with the appropriate length.
    string16 input;
    for (int ch = 0; ch < i; ch++)
      input.push_back('a');

    url_canon::RawCanonOutput<static_size> output;
    converter.ConvertFromUTF16(input.c_str(), static_cast<int>(input.length()),
                               &output);
    EXPECT_EQ(input.length(), static_cast<size_t>(output.length()));
  }
}

TEST(URLCanonTest, Scheme) {
  // Here, we're mostly testing that unusual characters are handled properly.
  // The canonicalizer doesn't do any parsing or whitespace detection. It will
  // also do its best on error, and will escape funny sequences (these won't be
  // valid schemes and it will return error).
  //
  // Note that the canonicalizer will append a colon to the output to separate
  // out the rest of the URL, which is not present in the input. We check,
  // however, that the output range includes everything but the colon.
  ComponentCase scheme_cases[] = {
    {"http", "http:", url_parse::Component(0, 4), true},
    {"HTTP", "http:", url_parse::Component(0, 4), true},
    {" HTTP ", "%20http%20:", url_parse::Component(0, 10), false},
    {"htt: ", "htt%3A%20:", url_parse::Component(0, 9), false},
    {"\xe4\xbd\xa0\xe5\xa5\xbdhttp", "%E4%BD%A0%E5%A5%BDhttp:", url_parse::Component(0, 22), false},
      // Don't re-escape something already escaped. Note that it will
      // "canonicalize" the 'A' to 'a', but that's OK.
    {"ht%3Atp", "ht%3atp:", url_parse::Component(0, 7), false},
  };

  std::string out_str;

  for (size_t i = 0; i < arraysize(scheme_cases); i++) {
    int url_len = static_cast<int>(strlen(scheme_cases[i].input));
    url_parse::Component in_comp(0, url_len);
    url_parse::Component out_comp;

    out_str.clear();
    url_canon::StdStringCanonOutput output1(&out_str);
    bool success = url_canon::CanonicalizeScheme(scheme_cases[i].input,
                                                 in_comp, &output1, &out_comp);
    output1.Complete();

    EXPECT_EQ(scheme_cases[i].expected_success, success);
    EXPECT_EQ(std::string(scheme_cases[i].expected), out_str);
    EXPECT_EQ(scheme_cases[i].expected_component.begin, out_comp.begin);
    EXPECT_EQ(scheme_cases[i].expected_component.len, out_comp.len);

    // Now try the wide version
    out_str.clear();
    url_canon::StdStringCanonOutput output2(&out_str);

    string16 wide_input(ConvertUTF8ToUTF16(scheme_cases[i].input));
    in_comp.len = static_cast<int>(wide_input.length());
    success = url_canon::CanonicalizeScheme(wide_input.c_str(), in_comp,
                                            &output2, &out_comp);
    output2.Complete();

    EXPECT_EQ(scheme_cases[i].expected_success, success);
    EXPECT_EQ(std::string(scheme_cases[i].expected), out_str);
    EXPECT_EQ(scheme_cases[i].expected_component.begin, out_comp.begin);
    EXPECT_EQ(scheme_cases[i].expected_component.len, out_comp.len);
  }

  // Test the case where the scheme is declared nonexistant, it should be
  // converted into an empty scheme.
  url_parse::Component out_comp;
  out_str.clear();
  url_canon::StdStringCanonOutput output(&out_str);

  EXPECT_TRUE(url_canon::CanonicalizeScheme("", url_parse::Component(0, -1),
                                            &output, &out_comp));
  output.Complete();

  EXPECT_EQ(std::string(":"), out_str);
  EXPECT_EQ(0, out_comp.begin);
  EXPECT_EQ(0, out_comp.len);
}

TEST(URLCanonTest, Host) {
  IPAddressCase host_cases[] = {
       // Basic canonicalization, uppercase should be converted to lowercase.
    {"GoOgLe.CoM", L"GoOgLe.CoM", "google.com", url_parse::Component(0, 10), CanonHostInfo::NEUTRAL, -1, ""},
      // Spaces and some other characters should be escaped.
    {"Goo%20 goo%7C|.com", L"Goo%20 goo%7C|.com", "goo%20%20goo%7C%7C.com", url_parse::Component(0, 22), CanonHostInfo::NEUTRAL, -1, ""},
      // Exciting different types of spaces!
    {NULL, L"GOO\x00a0\x3000goo.com", "goo%20%20goo.com", url_parse::Component(0, 16), CanonHostInfo::NEUTRAL, -1, ""},
      // Other types of space (no-break, zero-width, zero-width-no-break) are
      // name-prepped away to nothing.
    {NULL, L"GOO\x200b\x2060\xfeffgoo.com", "googoo.com", url_parse::Component(0, 10), CanonHostInfo::NEUTRAL, -1, ""},
      // Ideographic full stop (full-width period for Chinese, etc.) should be
      // treated as a dot.
    {NULL, L"www.foo\x3002"L"bar.com", "www.foo.bar.com", url_parse::Component(0, 15), CanonHostInfo::NEUTRAL, -1, ""},
      // Invalid unicode characters should fail...
      // ...In wide input, ICU will barf and we'll end up with the input as
      //    escaped UTF-8 (the invalid character should be replaced with the
      //    replacement character).
    {"\xef\xb7\x90zyx.com", L"\xfdd0zyx.com", "%EF%BF%BDzyx.com", url_parse::Component(0, 16), CanonHostInfo::BROKEN, -1, ""},
      // ...This is the same as previous but with with escaped.
    {"%ef%b7%90zyx.com", L"%ef%b7%90zyx.com", "%EF%BF%BDzyx.com", url_parse::Component(0, 16), CanonHostInfo::BROKEN, -1, ""},
      // Test name prepping, fullwidth input should be converted to ASCII and NOT
      // IDN-ized. This is "Go" in fullwidth UTF-8/UTF-16.
    {"\xef\xbc\xa7\xef\xbd\x8f.com", L"\xff27\xff4f.com", "go.com", url_parse::Component(0, 6), CanonHostInfo::NEUTRAL, -1, ""},
      // Test that fullwidth escaped values are properly name-prepped,
      // then converted or rejected.
      // ...%41 in fullwidth = 'A' (also as escaped UTF-8 input)
    {"\xef\xbc\x85\xef\xbc\x94\xef\xbc\x91.com", L"\xff05\xff14\xff11.com", "a.com", url_parse::Component(0, 5), CanonHostInfo::NEUTRAL, -1, ""},
    {"%ef%bc%85%ef%bc%94%ef%bc%91.com", L"%ef%bc%85%ef%bc%94%ef%bc%91.com", "a.com", url_parse::Component(0, 5), CanonHostInfo::NEUTRAL, -1, ""},
      // ...%00 in fullwidth should fail (also as escaped UTF-8 input)
    {"\xef\xbc\x85\xef\xbc\x90\xef\xbc\x90.com", L"\xff05\xff10\xff10.com", "%00.com", url_parse::Component(0, 7), CanonHostInfo::BROKEN, -1, ""},
    {"%ef%bc%85%ef%bc%90%ef%bc%90.com", L"%ef%bc%85%ef%bc%90%ef%bc%90.com", "%00.com", url_parse::Component(0, 7), CanonHostInfo::BROKEN, -1, ""},
      // ICU will convert weird percents into ASCII percents, but not unescape
      // further. A weird percent is U+FE6A (EF B9 AA in UTF-8) which is a
      // "small percent". At this point we should be within our rights to mark
      // anything as invalid since the URL is corrupt or malicious. The code
      // happens to allow ASCII characters (%41 = "A" -> 'a') to be unescaped
      // and kept as valid, so we validate that behavior here, but this level
      // of fixing the input shouldn't be seen as required. "%81" is invalid.
    {"\xef\xb9\xaa" "41.com", L"\xfe6a" L"41.com", "a.com", Component(0, 5), CanonHostInfo::NEUTRAL, -1, ""},
    {"%ef%b9%aa" "41.com", L"\xfe6a" L"41.com", "a.com", Component(0, 5), CanonHostInfo::NEUTRAL, -1, ""},
    {"\xef\xb9\xaa" "81.com", L"\xfe6a" L"81.com", "%81.com", Component(0, 7), CanonHostInfo::BROKEN, -1, ""},
    {"%ef%b9%aa" "81.com", L"\xfe6a" L"81.com", "%81.com", Component(0, 7), CanonHostInfo::BROKEN, -1, ""},
      // Basic IDN support, UTF-8 and UTF-16 input should be converted to IDN
    {"\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd", L"\x4f60\x597d\x4f60\x597d", "xn--6qqa088eba", url_parse::Component(0, 14), CanonHostInfo::NEUTRAL, -1, ""},
      // Mixed UTF-8 and escaped UTF-8 (narrow case) and UTF-16 and escaped
      // UTF-8 (wide case). The output should be equivalent to the true wide
      // character input above).
    {"%E4%BD%A0%E5%A5%BD\xe4\xbd\xa0\xe5\xa5\xbd", L"%E4%BD%A0%E5%A5%BD\x4f60\x597d", "xn--6qqa088eba", url_parse::Component(0, 14), CanonHostInfo::NEUTRAL, -1, ""},
      // Invalid escaped characters should fail and the percents should be
      // escaped.
    {"%zz%66%a", L"%zz%66%a", "%25zzf%25a", url_parse::Component(0, 10), CanonHostInfo::BROKEN, -1, ""},
      // If we get an invalid character that has been escaped.
    {"%25", L"%25", "%25", url_parse::Component(0, 3), CanonHostInfo::BROKEN, -1, ""},
    {"hello%00", L"hello%00", "hello%00", url_parse::Component(0, 8), CanonHostInfo::BROKEN, -1, ""},
      // Escaped numbers should be treated like IP addresses if they are.
    {"%30%78%63%30%2e%30%32%35%30.01", L"%30%78%63%30%2e%30%32%35%30.01", "192.168.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 3, "C0A80001"},
    {"%30%78%63%30%2e%30%32%35%30.01%2e", L"%30%78%63%30%2e%30%32%35%30.01%2e", "192.168.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 3, "C0A80001"},
      // Invalid escaping should trigger the regular host error handling.
    {"%3g%78%63%30%2e%30%32%35%30%2E.01", L"%3g%78%63%30%2e%30%32%35%30%2E.01", "%253gxc0.0250..01", url_parse::Component(0, 17), CanonHostInfo::BROKEN, -1, ""},
      // Something that isn't exactly an IP should get treated as a host and
      // spaces escaped.
    {"192.168.0.1 hello", L"192.168.0.1 hello", "192.168.0.1%20hello", url_parse::Component(0, 19), CanonHostInfo::NEUTRAL, -1, ""},
      // Fullwidth and escaped UTF-8 fullwidth should still be treated as IP.
      // These are "0Xc0.0250.01" in fullwidth.
    {"\xef\xbc\x90%Ef%bc\xb8%ef%Bd%83\xef\xbc\x90%EF%BC%8E\xef\xbc\x90\xef\xbc\x92\xef\xbc\x95\xef\xbc\x90\xef\xbc%8E\xef\xbc\x90\xef\xbc\x91", L"\xff10\xff38\xff43\xff10\xff0e\xff10\xff12\xff15\xff10\xff0e\xff10\xff11", "192.168.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 3, "C0A80001"},
      // Broken IP addresses get marked as such.
    {"192.168.0.257", L"192.168.0.257", "192.168.0.257", url_parse::Component(0, 13), CanonHostInfo::BROKEN, -1, ""},
    {"[google.com]", L"[google.com]", "[google.com]", url_parse::Component(0, 12), CanonHostInfo::BROKEN, -1, ""},
      // Cyrillic letter followed buy ( should return punicode for ( escaped before punicode string was created. I.e.
      // if ( is escaped after punicode is created we would get xn--%28-8tb (incorrect).
    {"\xd1\x82(", L"\x0442(", "xn--%28-7ed", url_parse::Component(0, 11), CanonHostInfo::NEUTRAL, -1, ""},
      // Address with all hexidecimal characters with leading number of 1<<32
      // or greater and should return NEUTRAL rather than BROKEN if not all
      // components are numbers.
    {"12345678912345.de", L"12345678912345.de", "12345678912345.de", url_parse::Component(0, 17), CanonHostInfo::NEUTRAL, -1, ""},
    {"1.12345678912345.de", L"1.12345678912345.de", "1.12345678912345.de", url_parse::Component(0, 19), CanonHostInfo::NEUTRAL, -1, ""},
    {"12345678912345.12345678912345.de", L"12345678912345.12345678912345.de", "12345678912345.12345678912345.de", url_parse::Component(0, 32), CanonHostInfo::NEUTRAL, -1, ""},
    {"1.2.0xB3A73CE5B59.de", L"1.2.0xB3A73CE5B59.de", "1.2.0xb3a73ce5b59.de", url_parse::Component(0, 20), CanonHostInfo::NEUTRAL, -1, ""},
    {"12345678912345.0xde", L"12345678912345.0xde", "12345678912345.0xde", url_parse::Component(0, 19), CanonHostInfo::BROKEN, -1, ""},
  };

  // CanonicalizeHost() non-verbose.
  std::string out_str;
  for (size_t i = 0; i < arraysize(host_cases); i++) {
    // Narrow version.
    if (host_cases[i].input8) {
      int host_len = static_cast<int>(strlen(host_cases[i].input8));
      url_parse::Component in_comp(0, host_len);
      url_parse::Component out_comp;

      out_str.clear();
      url_canon::StdStringCanonOutput output(&out_str);

      bool success = url_canon::CanonicalizeHost(host_cases[i].input8, in_comp,
                                                 &output, &out_comp);
      output.Complete();

      EXPECT_EQ(host_cases[i].expected_family != CanonHostInfo::BROKEN,
                success);
      EXPECT_EQ(std::string(host_cases[i].expected), out_str);
      EXPECT_EQ(host_cases[i].expected_component.begin, out_comp.begin);
      EXPECT_EQ(host_cases[i].expected_component.len, out_comp.len);
    }

    // Wide version.
    if (host_cases[i].input16) {
      string16 input16(WStringToUTF16(host_cases[i].input16));
      int host_len = static_cast<int>(input16.length());
      url_parse::Component in_comp(0, host_len);
      url_parse::Component out_comp;

      out_str.clear();
      url_canon::StdStringCanonOutput output(&out_str);

      bool success = url_canon::CanonicalizeHost(input16.c_str(), in_comp,
                                                 &output, &out_comp);
      output.Complete();

      EXPECT_EQ(host_cases[i].expected_family != CanonHostInfo::BROKEN,
                success);
      EXPECT_EQ(std::string(host_cases[i].expected), out_str);
      EXPECT_EQ(host_cases[i].expected_component.begin, out_comp.begin);
      EXPECT_EQ(host_cases[i].expected_component.len, out_comp.len);
    }
  }

  // CanonicalizeHostVerbose()
  for (size_t i = 0; i < arraysize(host_cases); i++) {
    // Narrow version.
    if (host_cases[i].input8) {
      int host_len = static_cast<int>(strlen(host_cases[i].input8));
      url_parse::Component in_comp(0, host_len);

      out_str.clear();
      url_canon::StdStringCanonOutput output(&out_str);
      CanonHostInfo host_info;

      url_canon::CanonicalizeHostVerbose(host_cases[i].input8, in_comp,
                                         &output, &host_info);
      output.Complete();

      EXPECT_EQ(host_cases[i].expected_family, host_info.family);
      EXPECT_EQ(std::string(host_cases[i].expected), out_str);
      EXPECT_EQ(host_cases[i].expected_component.begin,
                host_info.out_host.begin);
      EXPECT_EQ(host_cases[i].expected_component.len, host_info.out_host.len);
      EXPECT_EQ(std::string(host_cases[i].expected_address_hex),
                BytesToHexString(host_info.address, host_info.AddressLength()));
      if (host_cases[i].expected_family == CanonHostInfo::IPV4) {
        EXPECT_EQ(host_cases[i].expected_num_ipv4_components,
                  host_info.num_ipv4_components);
      }
    }

    // Wide version.
    if (host_cases[i].input16) {
      string16 input16(WStringToUTF16(host_cases[i].input16));
      int host_len = static_cast<int>(input16.length());
      url_parse::Component in_comp(0, host_len);

      out_str.clear();
      url_canon::StdStringCanonOutput output(&out_str);
      CanonHostInfo host_info;

      url_canon::CanonicalizeHostVerbose(input16.c_str(), in_comp,
                                         &output, &host_info);
      output.Complete();

      EXPECT_EQ(host_cases[i].expected_family, host_info.family);
      EXPECT_EQ(std::string(host_cases[i].expected), out_str);
      EXPECT_EQ(host_cases[i].expected_component.begin,
                host_info.out_host.begin);
      EXPECT_EQ(host_cases[i].expected_component.len, host_info.out_host.len);
      EXPECT_EQ(std::string(host_cases[i].expected_address_hex),
                BytesToHexString(host_info.address, host_info.AddressLength()));
      if (host_cases[i].expected_family == CanonHostInfo::IPV4) {
        EXPECT_EQ(host_cases[i].expected_num_ipv4_components,
                  host_info.num_ipv4_components);
      }
    }
  }
}

TEST(URLCanonTest, IPv4) {
  IPAddressCase cases[] = {
      // Empty is not an IP address.
    {"", L"", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {".", L".", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
      // Regular IP addresses in different bases.
    {"192.168.0.1", L"192.168.0.1", "192.168.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 4, "C0A80001"},
    {"0300.0250.00.01", L"0300.0250.00.01", "192.168.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 4, "C0A80001"},
    {"0xC0.0Xa8.0x0.0x1", L"0xC0.0Xa8.0x0.0x1", "192.168.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 4, "C0A80001"},
      // Non-IP addresses due to invalid characters.
    {"192.168.9.com", L"192.168.9.com", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
      // Invalid characters for the base should be rejected.
    {"19a.168.0.1", L"19a.168.0.1", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"0308.0250.00.01", L"0308.0250.00.01", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"0xCG.0xA8.0x0.0x1", L"0xCG.0xA8.0x0.0x1", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
      // If there are not enough components, the last one should fill them out.
    {"192", L"192", "0.0.0.192", url_parse::Component(0, 9), CanonHostInfo::IPV4, 1, "000000C0"},
    {"0xC0a80001", L"0xC0a80001", "192.168.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 1, "C0A80001"},
    {"030052000001", L"030052000001", "192.168.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 1, "C0A80001"},
    {"000030052000001", L"000030052000001", "192.168.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 1, "C0A80001"},
    {"192.168", L"192.168", "192.0.0.168", url_parse::Component(0, 11), CanonHostInfo::IPV4, 2, "C00000A8"},
    {"192.0x00A80001", L"192.0x000A80001", "192.168.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 2, "C0A80001"},
    {"0xc0.052000001", L"0xc0.052000001", "192.168.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 2, "C0A80001"},
    {"192.168.1", L"192.168.1", "192.168.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 3, "C0A80001"},
      // Too many components means not an IP address.
    {"192.168.0.0.1", L"192.168.0.0.1", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
      // We allow a single trailing dot.
    {"192.168.0.1.", L"192.168.0.1.", "192.168.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 4, "C0A80001"},
    {"192.168.0.1. hello", L"192.168.0.1. hello", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"192.168.0.1..", L"192.168.0.1..", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
      // Two dots in a row means not an IP address.
    {"192.168..1", L"192.168..1", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
      // Any numerical overflow should be marked as BROKEN.
    {"0x100.0", L"0x100.0", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0x100.0.0", L"0x100.0.0", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0x100.0.0.0", L"0x100.0.0.0", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0x100.0.0", L"0.0x100.0.0", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0.0x100.0", L"0.0.0x100.0", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0.0.0x100", L"0.0.0.0x100", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0.0x10000", L"0.0.0x10000", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0.0x1000000", L"0.0x1000000", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0x100000000", L"0x100000000", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
      // Repeat the previous tests, minus 1, to verify boundaries.
    {"0xFF.0", L"0xFF.0", "255.0.0.0", url_parse::Component(0, 9), CanonHostInfo::IPV4, 2, "FF000000"},
    {"0xFF.0.0", L"0xFF.0.0", "255.0.0.0", url_parse::Component(0, 9), CanonHostInfo::IPV4, 3, "FF000000"},
    {"0xFF.0.0.0", L"0xFF.0.0.0", "255.0.0.0", url_parse::Component(0, 9), CanonHostInfo::IPV4, 4, "FF000000"},
    {"0.0xFF.0.0", L"0.0xFF.0.0", "0.255.0.0", url_parse::Component(0, 9), CanonHostInfo::IPV4, 4, "00FF0000"},
    {"0.0.0xFF.0", L"0.0.0xFF.0", "0.0.255.0", url_parse::Component(0, 9), CanonHostInfo::IPV4, 4, "0000FF00"},
    {"0.0.0.0xFF", L"0.0.0.0xFF", "0.0.0.255", url_parse::Component(0, 9), CanonHostInfo::IPV4, 4, "000000FF"},
    {"0.0.0xFFFF", L"0.0.0xFFFF", "0.0.255.255", url_parse::Component(0, 11), CanonHostInfo::IPV4, 3, "0000FFFF"},
    {"0.0xFFFFFF", L"0.0xFFFFFF", "0.255.255.255", url_parse::Component(0, 13), CanonHostInfo::IPV4, 2, "00FFFFFF"},
    {"0xFFFFFFFF", L"0xFFFFFFFF", "255.255.255.255", url_parse::Component(0, 15), CanonHostInfo::IPV4, 1, "FFFFFFFF"},
      // Old trunctations tests.  They're all "BROKEN" now.
    {"276.256.0xf1a2.077777", L"276.256.0xf1a2.077777", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"192.168.0.257", L"192.168.0.257", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"192.168.0xa20001", L"192.168.0xa20001", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"192.015052000001", L"192.015052000001", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"0X12C0a80001", L"0X12C0a80001", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"276.1.2", L"276.1.2", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
      // Spaces should be rejected.
    {"192.168.0.1 hello", L"192.168.0.1 hello", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
      // Very large numbers.
    {"0000000000000300.0x00000000000000fF.00000000000000001", L"0000000000000300.0x00000000000000fF.00000000000000001", "192.255.0.1", url_parse::Component(0, 11), CanonHostInfo::IPV4, 3, "C0FF0001"},
    {"0000000000000300.0xffffffffFFFFFFFF.3022415481470977", L"0000000000000300.0xffffffffFFFFFFFF.3022415481470977", "", url_parse::Component(0, 11), CanonHostInfo::BROKEN, -1, ""},
      // A number has no length limit, but long numbers can still overflow.
    {"00000000000000000001", L"00000000000000000001", "0.0.0.1", url_parse::Component(0, 7), CanonHostInfo::IPV4, 1, "00000001"},
    {"0000000000000000100000000000000001", L"0000000000000000100000000000000001", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
      // If a long component is non-numeric, it's a hostname, *not* a broken IP.
    {"0.0.0.000000000000000000z", L"0.0.0.000000000000000000z", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
    {"0.0.0.100000000000000000z", L"0.0.0.100000000000000000z", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
      // Truncation of all zeros should still result in 0.
    {"0.00.0x.0x0", L"0.00.0x.0x0", "0.0.0.0", url_parse::Component(0, 7), CanonHostInfo::IPV4, 4, "00000000"},
  };

  for (size_t i = 0; i < arraysize(cases); i++) {
    // 8-bit version.
    url_parse::Component component(0,
                                   static_cast<int>(strlen(cases[i].input8)));

    std::string out_str1;
    url_canon::StdStringCanonOutput output1(&out_str1);
    url_canon::CanonHostInfo host_info;
    url_canon::CanonicalizeIPAddress(cases[i].input8, component, &output1,
                                     &host_info);
    output1.Complete();

    EXPECT_EQ(cases[i].expected_family, host_info.family);
    EXPECT_EQ(std::string(cases[i].expected_address_hex),
              BytesToHexString(host_info.address, host_info.AddressLength()));
    if (host_info.family == CanonHostInfo::IPV4) {
      EXPECT_STREQ(cases[i].expected, out_str1.c_str());
      EXPECT_EQ(cases[i].expected_component.begin, host_info.out_host.begin);
      EXPECT_EQ(cases[i].expected_component.len, host_info.out_host.len);
      EXPECT_EQ(cases[i].expected_num_ipv4_components,
                host_info.num_ipv4_components);
    }

    // 16-bit version.
    string16 input16(WStringToUTF16(cases[i].input16));
    component = url_parse::Component(0, static_cast<int>(input16.length()));

    std::string out_str2;
    url_canon::StdStringCanonOutput output2(&out_str2);
    url_canon::CanonicalizeIPAddress(input16.c_str(), component, &output2,
                                     &host_info);
    output2.Complete();

    EXPECT_EQ(cases[i].expected_family, host_info.family);
    EXPECT_EQ(std::string(cases[i].expected_address_hex),
              BytesToHexString(host_info.address, host_info.AddressLength()));
    if (host_info.family == CanonHostInfo::IPV4) {
      EXPECT_STREQ(cases[i].expected, out_str2.c_str());
      EXPECT_EQ(cases[i].expected_component.begin, host_info.out_host.begin);
      EXPECT_EQ(cases[i].expected_component.len, host_info.out_host.len);
      EXPECT_EQ(cases[i].expected_num_ipv4_components,
                host_info.num_ipv4_components);
    }
  }
}

TEST(URLCanonTest, IPv6) {
  IPAddressCase cases[] = {
      // Empty is not an IP address.
    {"", L"", "", url_parse::Component(), CanonHostInfo::NEUTRAL, -1, ""},
      // Non-IPs with [:] characters are marked BROKEN.
    {":", L":", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[", L"[", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[:", L"[:", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"]", L"]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {":]", L":]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[]", L"[]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[:]", L"[:]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
      // Regular IP address is invalid without bounding '[' and ']'.
    {"2001:db8::1", L"2001:db8::1", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[2001:db8::1", L"[2001:db8::1", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"2001:db8::1]", L"2001:db8::1]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
      // Regular IP addresses.
    {"[::]", L"[::]", "[::]", url_parse::Component(0,4), CanonHostInfo::IPV6, -1, "00000000000000000000000000000000"},
    {"[::1]", L"[::1]", "[::1]", url_parse::Component(0,5), CanonHostInfo::IPV6, -1, "00000000000000000000000000000001"},
    {"[1::]", L"[1::]", "[1::]", url_parse::Component(0,5), CanonHostInfo::IPV6, -1, "00010000000000000000000000000000"},

    // Leading zeros should be stripped.
    {"[000:01:02:003:004:5:6:007]", L"[000:01:02:003:004:5:6:007]", "[0:1:2:3:4:5:6:7]", url_parse::Component(0,17), CanonHostInfo::IPV6, -1, "00000001000200030004000500060007"},

    // Upper case letters should be lowercased.
    {"[A:b:c:DE:fF:0:1:aC]", L"[A:b:c:DE:fF:0:1:aC]", "[a:b:c:de:ff:0:1:ac]", url_parse::Component(0,20), CanonHostInfo::IPV6, -1, "000A000B000C00DE00FF0000000100AC"},

    // The same address can be written with different contractions, but should
    // get canonicalized to the same thing.
    {"[1:0:0:2::3:0]", L"[1:0:0:2::3:0]", "[1::2:0:0:3:0]", url_parse::Component(0,14), CanonHostInfo::IPV6, -1, "00010000000000020000000000030000"},
    {"[1::2:0:0:3:0]", L"[1::2:0:0:3:0]", "[1::2:0:0:3:0]", url_parse::Component(0,14), CanonHostInfo::IPV6, -1, "00010000000000020000000000030000"},

    // Addresses with embedded IPv4.
    {"[::192.168.0.1]", L"[::192.168.0.1]", "[::c0a8:1]", url_parse::Component(0,10), CanonHostInfo::IPV6, -1, "000000000000000000000000C0A80001"},
    {"[::ffff:192.168.0.1]", L"[::ffff:192.168.0.1]", "[::ffff:c0a8:1]", url_parse::Component(0,15), CanonHostInfo::IPV6, -1, "00000000000000000000FFFFC0A80001"},
    {"[::eeee:192.168.0.1]", L"[::eeee:192.168.0.1]", "[::eeee:c0a8:1]", url_parse::Component(0, 15), CanonHostInfo::IPV6, -1, "00000000000000000000EEEEC0A80001"},
    {"[2001::192.168.0.1]", L"[2001::192.168.0.1]", "[2001::c0a8:1]", url_parse::Component(0, 14), CanonHostInfo::IPV6, -1, "200100000000000000000000C0A80001"},
    {"[1:2:192.168.0.1:5:6]", L"[1:2:192.168.0.1:5:6]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},

    // IPv4 with last component missing.
    {"[::ffff:192.1.2]", L"[::ffff:192.1.2]", "[::ffff:c001:2]", url_parse::Component(0,15), CanonHostInfo::IPV6, -1, "00000000000000000000FFFFC0010002"},

    // IPv4 using hex.
    // TODO(eroman): Should this format be disallowed?
    {"[::ffff:0xC0.0Xa8.0x0.0x1]", L"[::ffff:0xC0.0Xa8.0x0.0x1]", "[::ffff:c0a8:1]", url_parse::Component(0,15), CanonHostInfo::IPV6, -1, "00000000000000000000FFFFC0A80001"},

    // There may be zeros surrounding the "::" contraction.
    {"[0:0::0:0:8]", L"[0:0::0:0:8]", "[::8]", url_parse::Component(0,5), CanonHostInfo::IPV6, -1, "00000000000000000000000000000008"},

    {"[2001:db8::1]", L"[2001:db8::1]", "[2001:db8::1]", url_parse::Component(0,13), CanonHostInfo::IPV6, -1, "20010DB8000000000000000000000001"},

      // Can only have one "::" contraction in an IPv6 string literal.
    {"[2001::db8::1]", L"[2001::db8::1]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
      // No more than 2 consecutive ':'s.
    {"[2001:db8:::1]", L"[2001:db8:::1]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[:::]", L"[:::]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
      // Non-IP addresses due to invalid characters.
    {"[2001::.com]", L"[2001::.com]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
      // If there are not enough components, the last one should fill them out.
    // ... omitted at this time ...
      // Too many components means not an IP address.  Similarly with too few if using IPv4 compat or mapped addresses.
    {"[::192.168.0.0.1]", L"[::192.168.0.0.1]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[::ffff:192.168.0.0.1]", L"[::ffff:192.168.0.0.1]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[1:2:3:4:5:6:7:8:9]", L"[1:2:3:4:5:6:7:8:9]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    // Too many bits (even though 8 comonents, the last one holds 32 bits).
    {"[0:0:0:0:0:0:0:192.168.0.1]", L"[0:0:0:0:0:0:0:192.168.0.1]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},

    // Too many bits specified -- the contraction would have to be zero-length
    // to not exceed 128 bits.
    {"[1:2:3:4:5:6::192.168.0.1]", L"[1:2:3:4:5:6::192.168.0.1]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},

    // The contraction is for 16 bits of zero.
    {"[1:2:3:4:5:6::8]", L"[1:2:3:4:5:6::8]", "[1:2:3:4:5:6:0:8]", url_parse::Component(0,17), CanonHostInfo::IPV6, -1, "00010002000300040005000600000008"},

    // Cannot have a trailing colon.
    {"[1:2:3:4:5:6:7:8:]", L"[1:2:3:4:5:6:7:8:]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[1:2:3:4:5:6:192.168.0.1:]", L"[1:2:3:4:5:6:192.168.0.1:]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},

    // Cannot have negative numbers.
    {"[-1:2:3:4:5:6:7:8]", L"[-1:2:3:4:5:6:7:8]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},

    // Scope ID -- the URL may contain an optional ["%" <scope_id>] section.
    // The scope_id should be included in the canonicalized URL, and is an
    // unsigned decimal number.

    // Invalid because no ID was given after the percent.

    // Don't allow scope-id
    {"[1::%1]", L"[1::%1]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[1::%eth0]", L"[1::%eth0]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[1::%]", L"[1::%]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[%]", L"[%]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[::%:]", L"[::%:]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},

    // Don't allow leading or trailing colons.
    {"[:0:0::0:0:8]", L"[:0:0::0:0:8]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[0:0::0:0:8:]", L"[0:0::0:0:8:]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
    {"[:0:0::0:0:8:]", L"[:0:0::0:0:8:]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},

      // We allow a single trailing dot.
    // ... omitted at this time ...
      // Two dots in a row means not an IP address.
    {"[::192.168..1]", L"[::192.168..1]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
      // Any non-first components get truncated to one byte.
    // ... omitted at this time ...
      // Spaces should be rejected.
    {"[::1 hello]", L"[::1 hello]", "", url_parse::Component(), CanonHostInfo::BROKEN, -1, ""},
  };

  for (size_t i = 0; i < arraysize(cases); i++) {
    // 8-bit version.
    url_parse::Component component(0,
                                   static_cast<int>(strlen(cases[i].input8)));

    std::string out_str1;
    url_canon::StdStringCanonOutput output1(&out_str1);
    url_canon::CanonHostInfo host_info;
    url_canon::CanonicalizeIPAddress(cases[i].input8, component, &output1,
                                     &host_info);
    output1.Complete();

    EXPECT_EQ(cases[i].expected_family, host_info.family);
    EXPECT_EQ(std::string(cases[i].expected_address_hex),
              BytesToHexString(host_info.address, host_info.AddressLength())) << "iter " << i << " host " << cases[i].input8;
    if (host_info.family == CanonHostInfo::IPV6) {
      EXPECT_STREQ(cases[i].expected, out_str1.c_str());
      EXPECT_EQ(cases[i].expected_component.begin,
                host_info.out_host.begin);
      EXPECT_EQ(cases[i].expected_component.len, host_info.out_host.len);
    }

    // 16-bit version.
    string16 input16(WStringToUTF16(cases[i].input16));
    component = url_parse::Component(0, static_cast<int>(input16.length()));

    std::string out_str2;
    url_canon::StdStringCanonOutput output2(&out_str2);
    url_canon::CanonicalizeIPAddress(input16.c_str(), component, &output2,
                                     &host_info);
    output2.Complete();

    EXPECT_EQ(cases[i].expected_family, host_info.family);
    EXPECT_EQ(std::string(cases[i].expected_address_hex),
              BytesToHexString(host_info.address, host_info.AddressLength()));
    if (host_info.family == CanonHostInfo::IPV6) {
      EXPECT_STREQ(cases[i].expected, out_str2.c_str());
      EXPECT_EQ(cases[i].expected_component.begin, host_info.out_host.begin);
      EXPECT_EQ(cases[i].expected_component.len, host_info.out_host.len);
    }
  }
}

TEST(URLCanonTest, IPEmpty) {
  std::string out_str1;
  url_canon::StdStringCanonOutput output1(&out_str1);
  url_canon::CanonHostInfo host_info;

  // This tests tests.
  const char spec[] = "192.168.0.1";
  url_canon::CanonicalizeIPAddress(spec, url_parse::Component(),
                                   &output1, &host_info);
  EXPECT_FALSE(host_info.IsIPAddress());

  url_canon::CanonicalizeIPAddress(spec, url_parse::Component(0, 0),
                                   &output1, &host_info);
  EXPECT_FALSE(host_info.IsIPAddress());
}

TEST(URLCanonTest, UserInfo) {
  // Note that the canonicalizer should escape and treat empty components as
  // not being there.

  // We actually parse a full input URL so we can get the initial components.
  struct UserComponentCase {
    const char* input;
    const char* expected;
    url_parse::Component expected_username;
    url_parse::Component expected_password;
    bool expected_success;
  } user_info_cases[] = {
    {"http://user:pass@host.com/", "user:pass@", url_parse::Component(0, 4), url_parse::Component(5, 4), true},
    {"http://@host.com/", "", url_parse::Component(0, -1), url_parse::Component(0, -1), true},
    {"http://:@host.com/", "", url_parse::Component(0, -1), url_parse::Component(0, -1), true},
    {"http://foo:@host.com/", "foo@", url_parse::Component(0, 3), url_parse::Component(0, -1), true},
    {"http://:foo@host.com/", ":foo@", url_parse::Component(0, 0), url_parse::Component(1, 3), true},
    {"http://^ :$\t@host.com/", "%5E%20:$%09@", url_parse::Component(0, 6), url_parse::Component(7, 4), true},
    {"http://user:pass@/", "user:pass@", url_parse::Component(0, 4), url_parse::Component(5, 4), true},
    {"http://%2540:bar@domain.com/", "%2540:bar@", url_parse::Component(0, 5), url_parse::Component(6, 3), true },

      // IE7 compatability: old versions allowed backslashes in usernames, but
      // IE7 does not. We disallow it as well.
    {"ftp://me\\mydomain:pass@foo.com/", "", url_parse::Component(0, -1), url_parse::Component(0, -1), true},
  };

  for (size_t i = 0; i < ARRAYSIZE(user_info_cases); i++) {
    int url_len = static_cast<int>(strlen(user_info_cases[i].input));
    url_parse::Parsed parsed;
    url_parse::ParseStandardURL(user_info_cases[i].input, url_len, &parsed);
    url_parse::Component out_user, out_pass;
    std::string out_str;
    url_canon::StdStringCanonOutput output1(&out_str);

    bool success = url_canon::CanonicalizeUserInfo(user_info_cases[i].input,
                                                   parsed.username,
                                                   user_info_cases[i].input,
                                                   parsed.password,
                                                   &output1, &out_user,
                                                   &out_pass);
    output1.Complete();

    EXPECT_EQ(user_info_cases[i].expected_success, success);
    EXPECT_EQ(std::string(user_info_cases[i].expected), out_str);
    EXPECT_EQ(user_info_cases[i].expected_username.begin, out_user.begin);
    EXPECT_EQ(user_info_cases[i].expected_username.len, out_user.len);
    EXPECT_EQ(user_info_cases[i].expected_password.begin, out_pass.begin);
    EXPECT_EQ(user_info_cases[i].expected_password.len, out_pass.len);

    // Now try the wide version
    out_str.clear();
    url_canon::StdStringCanonOutput output2(&out_str);
    string16 wide_input(ConvertUTF8ToUTF16(user_info_cases[i].input));
    success = url_canon::CanonicalizeUserInfo(wide_input.c_str(),
                                              parsed.username,
                                              wide_input.c_str(),
                                              parsed.password,
                                              &output2, &out_user, &out_pass);
    output2.Complete();

    EXPECT_EQ(user_info_cases[i].expected_success, success);
    EXPECT_EQ(std::string(user_info_cases[i].expected), out_str);
    EXPECT_EQ(user_info_cases[i].expected_username.begin, out_user.begin);
    EXPECT_EQ(user_info_cases[i].expected_username.len, out_user.len);
    EXPECT_EQ(user_info_cases[i].expected_password.begin, out_pass.begin);
    EXPECT_EQ(user_info_cases[i].expected_password.len, out_pass.len);
  }
}

TEST(URLCanonTest, Port) {
  // We only need to test that the number gets properly put into the output
  // buffer. The parser unit tests will test scanning the number correctly.
  //
  // Note that the CanonicalizePort will always prepend a colon to the output
  // to separate it from the colon that it assumes preceeds it.
  struct PortCase {
    const char* input;
    int default_port;
    const char* expected;
    url_parse::Component expected_component;
    bool expected_success;
  } port_cases[] = {
      // Invalid input should be copied w/ failure.
    {"as df", 80, ":as%20df", url_parse::Component(1, 7), false},
    {"-2", 80, ":-2", url_parse::Component(1, 2), false},
      // Default port should be omitted.
    {"80", 80, "", url_parse::Component(0, -1), true},
    {"8080", 80, ":8080", url_parse::Component(1, 4), true},
      // PORT_UNSPECIFIED should mean always keep the port.
    {"80", url_parse::PORT_UNSPECIFIED, ":80", url_parse::Component(1, 2), true},
  };

  for (size_t i = 0; i < ARRAYSIZE(port_cases); i++) {
    int url_len = static_cast<int>(strlen(port_cases[i].input));
    url_parse::Component in_comp(0, url_len);
    url_parse::Component out_comp;
    std::string out_str;
    url_canon::StdStringCanonOutput output1(&out_str);
    bool success = url_canon::CanonicalizePort(port_cases[i].input, in_comp,
                                               port_cases[i].default_port,
                                               &output1, &out_comp);
    output1.Complete();

    EXPECT_EQ(port_cases[i].expected_success, success);
    EXPECT_EQ(std::string(port_cases[i].expected), out_str);
    EXPECT_EQ(port_cases[i].expected_component.begin, out_comp.begin);
    EXPECT_EQ(port_cases[i].expected_component.len, out_comp.len);

    // Now try the wide version
    out_str.clear();
    url_canon::StdStringCanonOutput output2(&out_str);
    string16 wide_input(ConvertUTF8ToUTF16(port_cases[i].input));
    success = url_canon::CanonicalizePort(wide_input.c_str(), in_comp,
                                          port_cases[i].default_port,
                                          &output2, &out_comp);
    output2.Complete();

    EXPECT_EQ(port_cases[i].expected_success, success);
    EXPECT_EQ(std::string(port_cases[i].expected), out_str);
    EXPECT_EQ(port_cases[i].expected_component.begin, out_comp.begin);
    EXPECT_EQ(port_cases[i].expected_component.len, out_comp.len);
  }
}

TEST(URLCanonTest, Path) {
  DualComponentCase path_cases[] = {
    // ----- path collapsing tests -----
    {"/././foo", L"/././foo", "/foo", url_parse::Component(0, 4), true},
    {"/./.foo", L"/./.foo", "/.foo", url_parse::Component(0, 5), true},
    {"/foo/.", L"/foo/.", "/foo/", url_parse::Component(0, 5), true},
    {"/foo/./", L"/foo/./", "/foo/", url_parse::Component(0, 5), true},
      // double dots followed by a slash or the end of the string count
    {"/foo/bar/..", L"/foo/bar/..", "/foo/", url_parse::Component(0, 5), true},
    {"/foo/bar/../", L"/foo/bar/../", "/foo/", url_parse::Component(0, 5), true},
      // don't count double dots when they aren't followed by a slash
    {"/foo/..bar", L"/foo/..bar", "/foo/..bar", url_parse::Component(0, 10), true},
      // some in the middle
    {"/foo/bar/../ton", L"/foo/bar/../ton", "/foo/ton", url_parse::Component(0, 8), true},
    {"/foo/bar/../ton/../../a", L"/foo/bar/../ton/../../a", "/a", url_parse::Component(0, 2), true},
      // we should not be able to go above the root
    {"/foo/../../..", L"/foo/../../..", "/", url_parse::Component(0, 1), true},
    {"/foo/../../../ton", L"/foo/../../../ton", "/ton", url_parse::Component(0, 4), true},
      // escaped dots should be unescaped and treated the same as dots
    {"/foo/%2e", L"/foo/%2e", "/foo/", url_parse::Component(0, 5), true},
    {"/foo/%2e%2", L"/foo/%2e%2", "/foo/.%2", url_parse::Component(0, 8), true},
    {"/foo/%2e./%2e%2e/.%2e/%2e.bar", L"/foo/%2e./%2e%2e/.%2e/%2e.bar", "/..bar", url_parse::Component(0, 6), true},
      // Multiple slashes in a row should be preserved and treated like empty
      // directory names.
    {"////../..", L"////../..", "//", url_parse::Component(0, 2), true},

    // ----- escaping tests -----
    {"/foo", L"/foo", "/foo", url_parse::Component(0, 4), true},
      // Valid escape sequence
    {"/%20foo", L"/%20foo", "/%20foo", url_parse::Component(0, 7), true},
      // Invalid escape sequence we should pass through unchanged.
    {"/foo%", L"/foo%", "/foo%", url_parse::Component(0, 5), true},
    {"/foo%2", L"/foo%2", "/foo%2", url_parse::Component(0, 6), true},
      // Invalid escape sequence: bad characters should be treated the same as
      // the sourrounding text, not as escaped (in this case, UTF-8).
    {"/foo%2zbar", L"/foo%2zbar", "/foo%2zbar", url_parse::Component(0, 10), true},
    {"/foo%2\xc2\xa9zbar", NULL, "/foo%2%C2%A9zbar", url_parse::Component(0, 16), true},
    {NULL, L"/foo%2\xc2\xa9zbar", "/foo%2%C3%82%C2%A9zbar", url_parse::Component(0, 22), true},
      // Regular characters that are escaped should be unescaped
    {"/foo%41%7a", L"/foo%41%7a", "/fooAz", url_parse::Component(0, 6), true},
      // Funny characters that are unescaped should be escaped
    {"/foo\x09\x91%91", NULL, "/foo%09%91%91", url_parse::Component(0, 13), true},
    {NULL, L"/foo\x09\x91%91", "/foo%09%C2%91%91", url_parse::Component(0, 16), true},
      // Invalid characters that are escaped should cause a failure.
    {"/foo%00%51", L"/foo%00%51", "/foo%00Q", url_parse::Component(0, 8), false},
      // Some characters should be passed through unchanged regardless of esc.
    {"/(%28:%3A%29)", L"/(%28:%3A%29)", "/(%28:%3A%29)", url_parse::Component(0, 13), true},
      // Characters that are properly escaped should not have the case changed
      // of hex letters.
    {"/%3A%3a%3C%3c", L"/%3A%3a%3C%3c", "/%3A%3a%3C%3c", url_parse::Component(0, 13), true},
      // Funny characters that are unescaped should be escaped
    {"/foo\tbar", L"/foo\tbar", "/foo%09bar", url_parse::Component(0, 10), true},
      // Backslashes should get converted to forward slashes
    {"\\foo\\bar", L"\\foo\\bar", "/foo/bar", url_parse::Component(0, 8), true},
      // Hashes found in paths (possibly only when the caller explicitly sets
      // the path on an already-parsed URL) should be escaped.
    {"/foo#bar", L"/foo#bar", "/foo%23bar", url_parse::Component(0, 10), true},
      // %7f should be allowed and %3D should not be unescaped (these were wrong
      // in a previous version).
    {"/%7Ffp3%3Eju%3Dduvgw%3Dd", L"/%7Ffp3%3Eju%3Dduvgw%3Dd", "/%7Ffp3%3Eju%3Dduvgw%3Dd", url_parse::Component(0, 24), true},
      // @ should be passed through unchanged (escaped or unescaped).
    {"/@asdf%40", L"/@asdf%40", "/@asdf%40", url_parse::Component(0, 9), true},

    // ----- encoding tests -----
      // Basic conversions
    {"/\xe4\xbd\xa0\xe5\xa5\xbd\xe4\xbd\xa0\xe5\xa5\xbd", L"/\x4f60\x597d\x4f60\x597d", "/%E4%BD%A0%E5%A5%BD%E4%BD%A0%E5%A5%BD", url_parse::Component(0, 37), true},
      // Invalid unicode characters should fail. We only do validation on
      // UTF-16 input, so this doesn't happen on 8-bit.
    {"/\xef\xb7\x90zyx", NULL, "/%EF%B7%90zyx", url_parse::Component(0, 13), true},
    {NULL, L"/\xfdd0zyx", "/%EF%BF%BDzyx", url_parse::Component(0, 13), false},
  };

  for (size_t i = 0; i < arraysize(path_cases); i++) {
    if (path_cases[i].input8) {
      int len = static_cast<int>(strlen(path_cases[i].input8));
      url_parse::Component in_comp(0, len);
      url_parse::Component out_comp;
      std::string out_str;
      url_canon::StdStringCanonOutput output(&out_str);
      bool success = url_canon::CanonicalizePath(path_cases[i].input8, in_comp,
                                                 &output, &out_comp);
      output.Complete();

      EXPECT_EQ(path_cases[i].expected_success, success);
      EXPECT_EQ(path_cases[i].expected_component.begin, out_comp.begin);
      EXPECT_EQ(path_cases[i].expected_component.len, out_comp.len);
      EXPECT_EQ(path_cases[i].expected, out_str);
    }

    if (path_cases[i].input16) {
      string16 input16(WStringToUTF16(path_cases[i].input16));
      int len = static_cast<int>(input16.length());
      url_parse::Component in_comp(0, len);
      url_parse::Component out_comp;
      std::string out_str;
      url_canon::StdStringCanonOutput output(&out_str);

      bool success = url_canon::CanonicalizePath(input16.c_str(), in_comp,
                                                 &output, &out_comp);
      output.Complete();

      EXPECT_EQ(path_cases[i].expected_success, success);
      EXPECT_EQ(path_cases[i].expected_component.begin, out_comp.begin);
      EXPECT_EQ(path_cases[i].expected_component.len, out_comp.len);
      EXPECT_EQ(path_cases[i].expected, out_str);
    }
  }

  // Manual test: embedded NULLs should be escaped and the URL should be marked
  // as invalid.
  const char path_with_null[] = "/ab\0c";
  url_parse::Component in_comp(0, 5);
  url_parse::Component out_comp;

  std::string out_str;
  url_canon::StdStringCanonOutput output(&out_str);
  bool success = url_canon::CanonicalizePath(path_with_null, in_comp,
                                             &output, &out_comp);
  output.Complete();
  EXPECT_FALSE(success);
  EXPECT_EQ("/ab%00c", out_str);
}

TEST(URLCanonTest, Query) {
  struct QueryCase {
    const char* input8;
    const wchar_t* input16;
    const char* encoding;
    const char* expected;
  } query_cases[] = {
      // Regular ASCII case in some different encodings.
    {"foo=bar", L"foo=bar", NULL, "?foo=bar"},
    {"foo=bar", L"foo=bar", "utf-8", "?foo=bar"},
    {"foo=bar", L"foo=bar", "shift_jis", "?foo=bar"},
    {"foo=bar", L"foo=bar", "gb2312", "?foo=bar"},
      // Allow question marks in the query without escaping
    {"as?df", L"as?df", NULL, "?as?df"},
      // Always escape '#' since it would mark the ref.
    {"as#df", L"as#df", NULL, "?as%23df"},
      // Escape some questionable 8-bit characters, but never unescape.
    {"\x02hello\x7f bye", L"\x02hello\x7f bye", NULL, "?%02hello%7F%20bye"},
    {"%40%41123", L"%40%41123", NULL, "?%40%41123"},
      // Chinese input/output
    {"q=\xe4\xbd\xa0\xe5\xa5\xbd", L"q=\x4f60\x597d", NULL, "?q=%E4%BD%A0%E5%A5%BD"},
    {"q=\xe4\xbd\xa0\xe5\xa5\xbd", L"q=\x4f60\x597d", "gb2312", "?q=%C4%E3%BA%C3"},
    {"q=\xe4\xbd\xa0\xe5\xa5\xbd", L"q=\x4f60\x597d", "big5", "?q=%A7A%A6n"},
      // Unencodable character in the destination character set should be
      // escaped. The escape sequence unescapes to be the entity name:
      // "?q=&#20320;"
    {"q=Chinese\xef\xbc\xa7", L"q=Chinese\xff27", "iso-8859-1", "?q=Chinese%26%2365319%3B"},
      // Invalid UTF-8/16 input should be replaced with invalid characters.
    {"q=\xed\xed", L"q=\xd800\xd800", NULL, "?q=%EF%BF%BD%EF%BF%BD"},
      // Don't allow < or > because sometimes they are used for XSS if the
      // URL is echoed in content. Firefox does this, IE doesn't.
    {"q=<asdf>", L"q=<asdf>", NULL, "?q=%3Casdf%3E"},
      // Escape double quotemarks in the query.
    {"q=\"asdf\"", L"q=\"asdf\"", NULL, "?q=%22asdf%22"},
  };

  for (size_t i = 0; i < ARRAYSIZE(query_cases); i++) {
    url_parse::Component out_comp;

    UConvScoper conv(query_cases[i].encoding);
    ASSERT_TRUE(!query_cases[i].encoding || conv.converter());
    url_canon::ICUCharsetConverter converter(conv.converter());

    // Map NULL to a NULL converter pointer.
    url_canon::ICUCharsetConverter* conv_pointer = &converter;
    if (!query_cases[i].encoding)
      conv_pointer = NULL;

    if (query_cases[i].input8) {
      int len = static_cast<int>(strlen(query_cases[i].input8));
      url_parse::Component in_comp(0, len);
      std::string out_str;

      url_canon::StdStringCanonOutput output(&out_str);
      url_canon::CanonicalizeQuery(query_cases[i].input8, in_comp,
                                   conv_pointer, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(query_cases[i].expected, out_str);
    }

    if (query_cases[i].input16) {
      string16 input16(WStringToUTF16(query_cases[i].input16));
      int len = static_cast<int>(input16.length());
      url_parse::Component in_comp(0, len);
      std::string out_str;

      url_canon::StdStringCanonOutput output(&out_str);
      url_canon::CanonicalizeQuery(input16.c_str(), in_comp,
                                   conv_pointer, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(query_cases[i].expected, out_str);
    }
  }

  // Extra test for input with embedded NULL;
  std::string out_str;
  url_canon::StdStringCanonOutput output(&out_str);
  url_parse::Component out_comp;
  url_canon::CanonicalizeQuery("a \x00z\x01", url_parse::Component(0, 5), NULL,
                               &output, &out_comp);
  output.Complete();
  EXPECT_EQ("?a%20%00z%01", out_str);
}

TEST(URLCanonTest, Ref) {
  // Refs are trivial, it just checks the encoding.
  DualComponentCase ref_cases[] = {
      // Regular one, we shouldn't escape spaces, et al.
    {"hello, world", L"hello, world", "#hello, world", url_parse::Component(1, 12), true},
      // UTF-8/wide input should be preserved
    {"\xc2\xa9", L"\xa9", "#\xc2\xa9", url_parse::Component(1, 2), true},
      // Test a characer that takes > 16 bits (U+10300 = old italic letter A)
    {"\xF0\x90\x8C\x80ss", L"\xd800\xdf00ss", "#\xF0\x90\x8C\x80ss", url_parse::Component(1, 6), true},
      // Escaping should be preserved unchanged, even invalid ones
    {"%41%a", L"%41%a", "#%41%a", url_parse::Component(1, 5), true},
      // Invalid UTF-8/16 input should be flagged and the input made valid
    {"\xc2", NULL, "#\xef\xbf\xbd", url_parse::Component(1, 3), true},
    {NULL, L"\xd800\x597d", "#\xef\xbf\xbd\xe5\xa5\xbd", url_parse::Component(1, 6), true},
      // Test a Unicode invalid character.
    {"a\xef\xb7\x90", L"a\xfdd0", "#a\xef\xbf\xbd", url_parse::Component(1, 4), true},
      // Refs can have # signs and we should preserve them.
    {"asdf#qwer", L"asdf#qwer", "#asdf#qwer", url_parse::Component(1, 9), true},
    {"#asdf", L"#asdf", "##asdf", url_parse::Component(1, 5), true},
  };

  for (size_t i = 0; i < arraysize(ref_cases); i++) {
    // 8-bit input
    if (ref_cases[i].input8) {
      int len = static_cast<int>(strlen(ref_cases[i].input8));
      url_parse::Component in_comp(0, len);
      url_parse::Component out_comp;

      std::string out_str;
      url_canon::StdStringCanonOutput output(&out_str);
      url_canon::CanonicalizeRef(ref_cases[i].input8, in_comp,
                                 &output, &out_comp);
      output.Complete();

      EXPECT_EQ(ref_cases[i].expected_component.begin, out_comp.begin);
      EXPECT_EQ(ref_cases[i].expected_component.len, out_comp.len);
      EXPECT_EQ(ref_cases[i].expected, out_str);
    }

    // 16-bit input
    if (ref_cases[i].input16) {
      string16 input16(WStringToUTF16(ref_cases[i].input16));
      int len = static_cast<int>(input16.length());
      url_parse::Component in_comp(0, len);
      url_parse::Component out_comp;

      std::string out_str;
      url_canon::StdStringCanonOutput output(&out_str);
      url_canon::CanonicalizeRef(input16.c_str(), in_comp, &output, &out_comp);
      output.Complete();

      EXPECT_EQ(ref_cases[i].expected_component.begin, out_comp.begin);
      EXPECT_EQ(ref_cases[i].expected_component.len, out_comp.len);
      EXPECT_EQ(ref_cases[i].expected, out_str);
    }
  }

  // Try one with an embedded NULL. It should be stripped.
  const char null_input[5] = "ab\x00z";
  url_parse::Component null_input_component(0, 4);
  url_parse::Component out_comp;

  std::string out_str;
  url_canon::StdStringCanonOutput output(&out_str);
  url_canon::CanonicalizeRef(null_input, null_input_component,
                             &output, &out_comp);
  output.Complete();

  EXPECT_EQ(1, out_comp.begin);
  EXPECT_EQ(3, out_comp.len);
  EXPECT_EQ("#abz", out_str);
}

TEST(URLCanonTest, CanonicalizeStandardURL) {
  // The individual component canonicalize tests should have caught the cases
  // for each of those components. Here, we just need to test that the various
  // parts are included or excluded properly, and have the correct separators.
  struct URLCase {
    const char* input;
    const char* expected;
    bool expected_success;
  } cases[] = {
    {"http://www.google.com/foo?bar=baz#", "http://www.google.com/foo?bar=baz#", true},
    {"http://[www.google.com]/", "http://[www.google.com]/", false},
    {"ht\ttp:@www.google.com:80/;p?#", "ht%09tp://www.google.com:80/;p?#", false},
    {"http:////////user:@google.com:99?foo", "http://user@google.com:99/?foo", true},
    {"www.google.com", ":www.google.com/", true},
    {"http://192.0x00A80001", "http://192.168.0.1/", true},
    {"http://www/foo%2Ehtml", "http://www/foo.html", true},
    {"http://user:pass@/", "http://user:pass@/", false},
    {"http://%25DOMAIN:foobar@foodomain.com/", "http://%25DOMAIN:foobar@foodomain.com/", true},

      // Backslashes should get converted to forward slashes.
    {"http:\\\\www.google.com\\foo", "http://www.google.com/foo", true},

      // Busted refs shouldn't make the whole thing fail.
    {"http://www.google.com/asdf#\xc2", "http://www.google.com/asdf#\xef\xbf\xbd", true},

      // Basic port tests.
    {"http://foo:80/", "http://foo/", true},
    {"http://foo:81/", "http://foo:81/", true},
    {"httpa://foo:80/", "httpa://foo:80/", true},
    {"http://foo:-80/", "http://foo:-80/", false},

    {"https://foo:443/", "https://foo/", true},
    {"https://foo:80/", "https://foo:80/", true},
    {"ftp://foo:21/", "ftp://foo/", true},
    {"ftp://foo:80/", "ftp://foo:80/", true},
    {"gopher://foo:70/", "gopher://foo/", true},
    {"gopher://foo:443/", "gopher://foo:443/", true},
    {"ws://foo:80/", "ws://foo/", true},
    {"ws://foo:81/", "ws://foo:81/", true},
    {"ws://foo:443/", "ws://foo:443/", true},
    {"ws://foo:815/", "ws://foo:815/", true},
    {"wss://foo:80/", "wss://foo:80/", true},
    {"wss://foo:81/", "wss://foo:81/", true},
    {"wss://foo:443/", "wss://foo/", true},
    {"wss://foo:815/", "wss://foo:815/", true},

      // This particular code path ends up "backing up" to replace an invalid
      // host ICU generated with an escaped version. Test that in the context
      // of a full URL to make sure the backing up doesn't mess up the non-host
      // parts of the URL. "EF B9 AA" is U+FE6A which is a type of percent that
      // ICU will convert to an ASCII one, generating "%81".
    {"ws:)W\x1eW\xef\xb9\xaa""81:80/", "ws://%29w%1ew%81/", false},
  };

  for (size_t i = 0; i < ARRAYSIZE(cases); i++) {
    int url_len = static_cast<int>(strlen(cases[i].input));
    url_parse::Parsed parsed;
    url_parse::ParseStandardURL(cases[i].input, url_len, &parsed);

    url_parse::Parsed out_parsed;
    std::string out_str;
    url_canon::StdStringCanonOutput output(&out_str);
    bool success = url_canon::CanonicalizeStandardURL(
        cases[i].input, url_len, parsed, NULL, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(cases[i].expected_success, success);
    EXPECT_EQ(cases[i].expected, out_str);
  }
}

// The codepath here is the same as for regular canonicalization, so we just
// need to test that things are replaced or not correctly.
TEST(URLCanonTest, ReplaceStandardURL) {
  ReplaceCase replace_cases[] = {
      // Common case of truncating the path.
    {"http://www.google.com/foo?bar=baz#ref", NULL, NULL, NULL, NULL, NULL, "/", kDeleteComp, kDeleteComp, "http://www.google.com/"},
      // Replace everything
    {"http://a:b@google.com:22/foo;bar?baz@cat", "https", "me", "pw", "host.com", "99", "/path", "query", "ref", "https://me:pw@host.com:99/path?query#ref"},
      // Replace nothing
    {"http://a:b@google.com:22/foo?baz@cat", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "http://a:b@google.com:22/foo?baz@cat"},
      // Replace scheme with filesystem.  The result is garbage, but you asked
      // for it.
    {"http://a:b@google.com:22/foo?baz@cat", "filesystem", NULL, NULL, NULL, NULL, NULL, NULL, NULL, "filesystem://a:b@google.com:22/foo?baz@cat"},
  };

  for (size_t i = 0; i < arraysize(replace_cases); i++) {
    const ReplaceCase& cur = replace_cases[i];
    int base_len = static_cast<int>(strlen(cur.base));
    url_parse::Parsed parsed;
    url_parse::ParseStandardURL(cur.base, base_len, &parsed);

    url_canon::Replacements<char> r;
    typedef url_canon::Replacements<char> R;  // Clean up syntax.

    // Note that for the scheme we pass in a different clear function since
    // there is no function to clear the scheme.
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    url_canon::StdStringCanonOutput output(&out_str);
    url_parse::Parsed out_parsed;
    url_canon::ReplaceStandardURL(replace_cases[i].base, parsed,
                                  r, NULL, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_cases[i].expected, out_str);
  }

  // The path pointer should be ignored if the address is invalid.
  {
    const char src[] = "http://www.google.com/here_is_the_path";
    int src_len = static_cast<int>(strlen(src));

    url_parse::Parsed parsed;
    url_parse::ParseStandardURL(src, src_len, &parsed);

    // Replace the path to 0 length string. By using 1 as the string address,
    // the test should get an access violation if it tries to dereference it.
    url_canon::Replacements<char> r;
    r.SetPath(reinterpret_cast<char*>(0x00000001), url_parse::Component(0, 0));
    std::string out_str1;
    url_canon::StdStringCanonOutput output1(&out_str1);
    url_parse::Parsed new_parsed;
    url_canon::ReplaceStandardURL(src, parsed, r, NULL, &output1, &new_parsed);
    output1.Complete();
    EXPECT_STREQ("http://www.google.com/", out_str1.c_str());

    // Same with an "invalid" path.
    r.SetPath(reinterpret_cast<char*>(0x00000001), url_parse::Component());
    std::string out_str2;
    url_canon::StdStringCanonOutput output2(&out_str2);
    url_canon::ReplaceStandardURL(src, parsed, r, NULL, &output2, &new_parsed);
    output2.Complete();
    EXPECT_STREQ("http://www.google.com/", out_str2.c_str());
  }
}

TEST(URLCanonTest, ReplaceFileURL) {
  ReplaceCase replace_cases[] = {
      // Replace everything
    {"file:///C:/gaba?query#ref", NULL, NULL, NULL, "filer", NULL, "/foo", "b", "c", "file://filer/foo?b#c"},
      // Replace nothing
    {"file:///C:/gaba?query#ref", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "file:///C:/gaba?query#ref"},
      // Clear non-path components (common)
    {"file:///C:/gaba?query#ref", NULL, NULL, NULL, NULL, NULL, NULL, kDeleteComp, kDeleteComp, "file:///C:/gaba"},
      // Replace path with something that doesn't begin with a slash and make
      // sure it gets added properly.
    {"file:///C:/gaba", NULL, NULL, NULL, NULL, NULL, "interesting/", NULL, NULL, "file:///interesting/"},
    {"file:///home/gaba?query#ref", NULL, NULL, NULL, "filer", NULL, "/foo", "b", "c", "file://filer/foo?b#c"},
    {"file:///home/gaba?query#ref", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "file:///home/gaba?query#ref"},
    {"file:///home/gaba?query#ref", NULL, NULL, NULL, NULL, NULL, NULL, kDeleteComp, kDeleteComp, "file:///home/gaba"},
    {"file:///home/gaba", NULL, NULL, NULL, NULL, NULL, "interesting/", NULL, NULL, "file:///interesting/"},
      // Replace scheme -- shouldn't do anything.
    {"file:///C:/gaba?query#ref", "http", NULL, NULL, NULL, NULL, NULL, NULL, NULL, "file:///C:/gaba?query#ref"},
  };

  for (size_t i = 0; i < arraysize(replace_cases); i++) {
    const ReplaceCase& cur = replace_cases[i];
    int base_len = static_cast<int>(strlen(cur.base));
    url_parse::Parsed parsed;
    url_parse::ParseFileURL(cur.base, base_len, &parsed);

    url_canon::Replacements<char> r;
    typedef url_canon::Replacements<char> R;  // Clean up syntax.
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    url_canon::StdStringCanonOutput output(&out_str);
    url_parse::Parsed out_parsed;
    url_canon::ReplaceFileURL(cur.base, parsed,
                              r, NULL, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_cases[i].expected, out_str);
  }
}

TEST(URLCanonTest, ReplaceFileSystemURL) {
  ReplaceCase replace_cases[] = {
      // Replace everything in the outer URL.
    {"filesystem:file:///temporary/gaba?query#ref", NULL, NULL, NULL, NULL, NULL, "/foo", "b", "c", "filesystem:file:///temporary/foo?b#c"},
      // Replace nothing
    {"filesystem:file:///temporary/gaba?query#ref", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "filesystem:file:///temporary/gaba?query#ref"},
      // Clear non-path components (common)
    {"filesystem:file:///temporary/gaba?query#ref", NULL, NULL, NULL, NULL, NULL, NULL, kDeleteComp, kDeleteComp, "filesystem:file:///temporary/gaba"},
      // Replace path with something that doesn't begin with a slash and make
      // sure it gets added properly.
    {"filesystem:file:///temporary/gaba?query#ref", NULL, NULL, NULL, NULL, NULL, "interesting/", NULL, NULL, "filesystem:file:///temporary/interesting/?query#ref"},
      // Replace scheme -- shouldn't do anything.
    {"filesystem:http://u:p@bar.com/t/gaba?query#ref", "http", NULL, NULL, NULL, NULL, NULL, NULL, NULL, "filesystem:http://u:p@bar.com/t/gaba?query#ref"},
      // Replace username -- shouldn't do anything.
    {"filesystem:http://u:p@bar.com/t/gaba?query#ref", NULL, "u2", NULL, NULL, NULL, NULL, NULL, NULL, "filesystem:http://u:p@bar.com/t/gaba?query#ref"},
      // Replace password -- shouldn't do anything.
    {"filesystem:http://u:p@bar.com/t/gaba?query#ref", NULL, NULL, "pw2", NULL, NULL, NULL, NULL, NULL, "filesystem:http://u:p@bar.com/t/gaba?query#ref"},
      // Replace host -- shouldn't do anything.
    {"filesystem:http://u:p@bar.com/t/gaba?query#ref", NULL, NULL, NULL, "foo.com", NULL, NULL, NULL, NULL, "filesystem:http://u:p@bar.com/t/gaba?query#ref"},
      // Replace port -- shouldn't do anything.
    {"filesystem:http://u:p@bar.com:40/t/gaba?query#ref", NULL, NULL, NULL, NULL, "41", NULL, NULL, NULL, "filesystem:http://u:p@bar.com:40/t/gaba?query#ref"},
  };

  for (size_t i = 0; i < arraysize(replace_cases); i++) {
    const ReplaceCase& cur = replace_cases[i];
    int base_len = static_cast<int>(strlen(cur.base));
    url_parse::Parsed parsed;
    url_parse::ParseFileSystemURL(cur.base, base_len, &parsed);

    url_canon::Replacements<char> r;
    typedef url_canon::Replacements<char> R;  // Clean up syntax.
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    url_canon::StdStringCanonOutput output(&out_str);
    url_parse::Parsed out_parsed;
    url_canon::ReplaceFileSystemURL(cur.base, parsed, r, NULL,
                                    &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_cases[i].expected, out_str);
  }
}

TEST(URLCanonTest, ReplacePathURL) {
  ReplaceCase replace_cases[] = {
      // Replace everything
    {"data:foo", "javascript", NULL, NULL, NULL, NULL, "alert('foo?');", NULL, NULL, "javascript:alert('foo?');"},
      // Replace nothing
    {"data:foo", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "data:foo"},
      // Replace one or the other
    {"data:foo", "javascript", NULL, NULL, NULL, NULL, NULL, NULL, NULL, "javascript:foo"},
    {"data:foo", NULL, NULL, NULL, NULL, NULL, "bar", NULL, NULL, "data:bar"},
    {"data:foo", NULL, NULL, NULL, NULL, NULL, kDeleteComp, NULL, NULL, "data:"},
  };

  for (size_t i = 0; i < arraysize(replace_cases); i++) {
    const ReplaceCase& cur = replace_cases[i];
    int base_len = static_cast<int>(strlen(cur.base));
    url_parse::Parsed parsed;
    url_parse::ParsePathURL(cur.base, base_len, &parsed);

    url_canon::Replacements<char> r;
    typedef url_canon::Replacements<char> R;  // Clean up syntax.
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    url_canon::StdStringCanonOutput output(&out_str);
    url_parse::Parsed out_parsed;
    url_canon::ReplacePathURL(cur.base, parsed,
                              r, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_cases[i].expected, out_str);
  }
}

TEST(URLCanonTest, ReplaceMailtoURL) {
  ReplaceCase replace_cases[] = {
      // Replace everything
    {"mailto:jon@foo.com?body=sup", "mailto", NULL, NULL, NULL, NULL, "addr1", "to=tony", NULL, "mailto:addr1?to=tony"},
      // Replace nothing
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, "mailto:jon@foo.com?body=sup"},
      // Replace the path
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, "jason", NULL, NULL, "mailto:jason?body=sup"},
      // Replace the query
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, NULL, "custom=1", NULL, "mailto:jon@foo.com?custom=1"},
      // Replace the path and query
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, "jason", "custom=1", NULL, "mailto:jason?custom=1"},
      // Set the query to empty (should leave trailing question mark)
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, NULL, "", NULL, "mailto:jon@foo.com?"},
      // Clear the query
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, NULL, "|", NULL, "mailto:jon@foo.com"},
      // Clear the path
    {"mailto:jon@foo.com?body=sup", NULL, NULL, NULL, NULL, NULL, "|", NULL, NULL, "mailto:?body=sup"},
      // Clear the path + query
    {"mailto:", NULL, NULL, NULL, NULL, NULL, "|", "|", NULL, "mailto:"},
      // Setting the ref should have no effect
    {"mailto:addr1", NULL, NULL, NULL, NULL, NULL, NULL, NULL, "BLAH", "mailto:addr1"},
  };

  for (size_t i = 0; i < arraysize(replace_cases); i++) {
    const ReplaceCase& cur = replace_cases[i];
    int base_len = static_cast<int>(strlen(cur.base));
    url_parse::Parsed parsed;
    url_parse::ParseMailtoURL(cur.base, base_len, &parsed);

    url_canon::Replacements<char> r;
    typedef url_canon::Replacements<char> R;
    SetupReplComp(&R::SetScheme, &R::ClearRef, &r, cur.scheme);
    SetupReplComp(&R::SetUsername, &R::ClearUsername, &r, cur.username);
    SetupReplComp(&R::SetPassword, &R::ClearPassword, &r, cur.password);
    SetupReplComp(&R::SetHost, &R::ClearHost, &r, cur.host);
    SetupReplComp(&R::SetPort, &R::ClearPort, &r, cur.port);
    SetupReplComp(&R::SetPath, &R::ClearPath, &r, cur.path);
    SetupReplComp(&R::SetQuery, &R::ClearQuery, &r, cur.query);
    SetupReplComp(&R::SetRef, &R::ClearRef, &r, cur.ref);

    std::string out_str;
    url_canon::StdStringCanonOutput output(&out_str);
    url_parse::Parsed out_parsed;
    url_canon::ReplaceMailtoURL(cur.base, parsed,
                                r, &output, &out_parsed);
    output.Complete();

    EXPECT_EQ(replace_cases[i].expected, out_str);
  }
}

TEST(URLCanonTest, CanonicalizeFileURL) {
  struct URLCase {
    const char* input;
    const char* expected;
    bool expected_success;
    url_parse::Component expected_host;
    url_parse::Component expected_path;
  } cases[] = {
#ifdef WIN32
      // Windows-style paths
    {"file:c:\\foo\\bar.html", "file:///C:/foo/bar.html", true, url_parse::Component(), url_parse::Component(7, 16)},
    {"  File:c|////foo\\bar.html", "file:///C:////foo/bar.html", true, url_parse::Component(), url_parse::Component(7, 19)},
    {"file:", "file:///", true, url_parse::Component(), url_parse::Component(7, 1)},
    {"file:UNChost/path", "file://unchost/path", true, url_parse::Component(7, 7), url_parse::Component(14, 5)},
      // CanonicalizeFileURL supports absolute Windows style paths for IE
      // compatability. Note that the caller must decide that this is a file
      // URL itself so it can call the file canonicalizer. This is usually
      // done automatically as part of relative URL resolving.
    {"c:\\foo\\bar", "file:///C:/foo/bar", true, url_parse::Component(), url_parse::Component(7, 11)},
    {"C|/foo/bar", "file:///C:/foo/bar", true, url_parse::Component(), url_parse::Component(7, 11)},
    {"/C|\\foo\\bar", "file:///C:/foo/bar", true, url_parse::Component(), url_parse::Component(7, 11)},
    {"//C|/foo/bar", "file:///C:/foo/bar", true, url_parse::Component(), url_parse::Component(7, 11)},
    {"//server/file", "file://server/file", true, url_parse::Component(7, 6), url_parse::Component(13, 5)},
    {"\\\\server\\file", "file://server/file", true, url_parse::Component(7, 6), url_parse::Component(13, 5)},
    {"/\\server/file", "file://server/file", true, url_parse::Component(7, 6), url_parse::Component(13, 5)},
      // We should preserve the number of slashes after the colon for IE
      // compatability, except when there is none, in which case we should
      // add one.
    {"file:c:foo/bar.html", "file:///C:/foo/bar.html", true, url_parse::Component(), url_parse::Component(7, 16)},
    {"file:/\\/\\C:\\\\//foo\\bar.html", "file:///C:////foo/bar.html", true, url_parse::Component(), url_parse::Component(7, 19)},
      // Three slashes should be non-UNC, even if there is no drive spec (IE
      // does this, which makes the resulting request invalid).
    {"file:///foo/bar.txt", "file:///foo/bar.txt", true, url_parse::Component(), url_parse::Component(7, 12)},
      // TODO(brettw) we should probably fail for invalid host names, which
      // would change the expected result on this test. We also currently allow
      // colon even though it's probably invalid, because its currently the
      // "natural" result of the way the canonicalizer is written. There doesn't
      // seem to be a strong argument for why allowing it here would be bad, so
      // we just tolerate it and the load will fail later.
    {"FILE:/\\/\\7:\\\\//foo\\bar.html", "file://7:////foo/bar.html", false, url_parse::Component(7, 2), url_parse::Component(9, 16)},
    {"file:filer/home\\me", "file://filer/home/me", true, url_parse::Component(7, 5), url_parse::Component(12, 8)},
      // Make sure relative paths can't go above the "C:"
    {"file:///C:/foo/../../../bar.html", "file:///C:/bar.html", true, url_parse::Component(), url_parse::Component(7, 12)},
      // Busted refs shouldn't make the whole thing fail.
    {"file:///C:/asdf#\xc2", "file:///C:/asdf#\xef\xbf\xbd", true, url_parse::Component(), url_parse::Component(7, 8)},
#else
      // Unix-style paths
    {"file:///home/me", "file:///home/me", true, url_parse::Component(), url_parse::Component(7, 8)},
      // Windowsy ones should get still treated as Unix-style.
    {"file:c:\\foo\\bar.html", "file:///c:/foo/bar.html", true, url_parse::Component(), url_parse::Component(7, 16)},
    {"file:c|//foo\\bar.html", "file:///c%7C//foo/bar.html", true, url_parse::Component(), url_parse::Component(7, 19)},
      // file: tests from WebKit (LayoutTests/fast/loader/url-parse-1.html)
    {"//", "file:///", true, url_parse::Component(), url_parse::Component(7, 1)},
    {"///", "file:///", true, url_parse::Component(), url_parse::Component(7, 1)},
    {"///test", "file:///test", true, url_parse::Component(), url_parse::Component(7, 5)},
    {"file://test", "file://test/", true, url_parse::Component(7, 4), url_parse::Component(11, 1)},
    {"file://localhost",  "file://localhost/", true, url_parse::Component(7, 9), url_parse::Component(16, 1)},
    {"file://localhost/", "file://localhost/", true, url_parse::Component(7, 9), url_parse::Component(16, 1)},
    {"file://localhost/test", "file://localhost/test", true, url_parse::Component(7, 9), url_parse::Component(16, 5)},
#endif  // WIN32
  };

  for (size_t i = 0; i < ARRAYSIZE(cases); i++) {
    int url_len = static_cast<int>(strlen(cases[i].input));
    url_parse::Parsed parsed;
    url_parse::ParseFileURL(cases[i].input, url_len, &parsed);

    url_parse::Parsed out_parsed;
    std::string out_str;
    url_canon::StdStringCanonOutput output(&out_str);
    bool success = url_canon::CanonicalizeFileURL(cases[i].input, url_len,
                                                  parsed, NULL, &output,
                                                  &out_parsed);
    output.Complete();

    EXPECT_EQ(cases[i].expected_success, success);
    EXPECT_EQ(cases[i].expected, out_str);

    // Make sure the spec was properly identified, the file canonicalizer has
    // different code for writing the spec.
    EXPECT_EQ(0, out_parsed.scheme.begin);
    EXPECT_EQ(4, out_parsed.scheme.len);

    EXPECT_EQ(cases[i].expected_host.begin, out_parsed.host.begin);
    EXPECT_EQ(cases[i].expected_host.len, out_parsed.host.len);

    EXPECT_EQ(cases[i].expected_path.begin, out_parsed.path.begin);
    EXPECT_EQ(cases[i].expected_path.len, out_parsed.path.len);
  }
}

TEST(URLCanonTest, CanonicalizeFileSystemURL) {
  struct URLCase {
    const char* input;
    const char* expected;
    bool expected_success;
  } cases[] = {
    {"Filesystem:htTp://www.Foo.com:80/tempoRary", "filesystem:http://www.foo.com/tempoRary/", true},
    {"filesystem:httpS://www.foo.com/temporary/", "filesystem:https://www.foo.com/temporary/", true},
    {"filesystem:http://www.foo.com//", "filesystem:http://www.foo.com//", false},
    {"filesystem:http://www.foo.com/persistent/bob?query#ref", "filesystem:http://www.foo.com/persistent/bob?query#ref", true},
    {"filesystem:fIle://\\temporary/", "filesystem:file:///temporary/", true},
    {"filesystem:fiLe:///temporary", "filesystem:file:///temporary/", true},
    {"filesystem:File:///temporary/Bob?qUery#reF", "filesystem:file:///temporary/Bob?qUery#reF", true},
  };

  for (size_t i = 0; i < ARRAYSIZE(cases); i++) {
    int url_len = static_cast<int>(strlen(cases[i].input));
    url_parse::Parsed parsed;
    url_parse::ParseFileSystemURL(cases[i].input, url_len, &parsed);

    url_parse::Parsed out_parsed;
    std::string out_str;
    url_canon::StdStringCanonOutput output(&out_str);
    bool success = url_canon::CanonicalizeFileSystemURL(cases[i].input, url_len,
                                                        parsed, NULL, &output,
                                                        &out_parsed);
    output.Complete();

    EXPECT_EQ(cases[i].expected_success, success);
    EXPECT_EQ(cases[i].expected, out_str);

    // Make sure the spec was properly identified, the filesystem canonicalizer
    // has different code for writing the spec.
    EXPECT_EQ(0, out_parsed.scheme.begin);
    EXPECT_EQ(10, out_parsed.scheme.len);
    if (success)
      EXPECT_GT(out_parsed.path.len, 0);
  }
}

TEST(URLCanonTest, CanonicalizePathURL) {
  // Path URLs should get canonicalized schemes but nothing else.
  struct PathCase {
    const char* input;
    const char* expected;
  } path_cases[] = {
    {"javascript:", "javascript:"},
    {"JavaScript:Foo", "javascript:Foo"},
    {":\":This /is interesting;?#", ":\":This /is interesting;?#"},
  };

  for (size_t i = 0; i < ARRAYSIZE(path_cases); i++) {
    int url_len = static_cast<int>(strlen(path_cases[i].input));
    url_parse::Parsed parsed;
    url_parse::ParsePathURL(path_cases[i].input, url_len, &parsed);

    url_parse::Parsed out_parsed;
    std::string out_str;
    url_canon::StdStringCanonOutput output(&out_str);
    bool success = url_canon::CanonicalizePathURL(path_cases[i].input, url_len,
                                                  parsed, &output,
                                                  &out_parsed);
    output.Complete();

    EXPECT_TRUE(success);
    EXPECT_EQ(path_cases[i].expected, out_str);

    EXPECT_EQ(0, out_parsed.host.begin);
    EXPECT_EQ(-1, out_parsed.host.len);

    // When we end with a colon at the end, there should be no path.
    if (path_cases[i].input[url_len - 1] == ':') {
      EXPECT_EQ(0, out_parsed.path.begin);
      EXPECT_EQ(-1, out_parsed.path.len);
    }
  }
}

TEST(URLCanonTest, CanonicalizeMailtoURL) {
  struct URLCase {
    const char* input;
    const char* expected;
    bool expected_success;
    url_parse::Component expected_path;
    url_parse::Component expected_query;
  } cases[] = {
    {"mailto:addr1", "mailto:addr1", true, url_parse::Component(7, 5), url_parse::Component()},
    {"mailto:addr1@foo.com", "mailto:addr1@foo.com", true, url_parse::Component(7, 13), url_parse::Component()},
    // Trailing whitespace is stripped.
    {"MaIlTo:addr1 \t ", "mailto:addr1", true, url_parse::Component(7, 5), url_parse::Component()},
    {"MaIlTo:addr1?to=jon", "mailto:addr1?to=jon", true, url_parse::Component(7, 5), url_parse::Component(13,6)},
    {"mailto:addr1,addr2", "mailto:addr1,addr2", true, url_parse::Component(7, 11), url_parse::Component()},
    {"mailto:addr1, addr2", "mailto:addr1, addr2", true, url_parse::Component(7, 12), url_parse::Component()},
    {"mailto:addr1%2caddr2", "mailto:addr1%2caddr2", true, url_parse::Component(7, 13), url_parse::Component()},
    {"mailto:\xF0\x90\x8C\x80", "mailto:%F0%90%8C%80", true, url_parse::Component(7, 12), url_parse::Component()},
    // Null character should be escaped to %00
    {"mailto:addr1\0addr2?foo", "mailto:addr1%00addr2?foo", true, url_parse::Component(7, 13), url_parse::Component(21, 3)},
    // Invalid -- UTF-8 encoded surrogate value.
    {"mailto:\xed\xa0\x80", "mailto:%EF%BF%BD", false, url_parse::Component(7, 9), url_parse::Component()},
    {"mailto:addr1?", "mailto:addr1?", true, url_parse::Component(7, 5), url_parse::Component(13, 0)},
  };

  // Define outside of loop to catch bugs where components aren't reset
  url_parse::Parsed parsed;
  url_parse::Parsed out_parsed;

  for (size_t i = 0; i < ARRAYSIZE(cases); i++) {
    int url_len = static_cast<int>(strlen(cases[i].input));
    if (i == 8) {
      // The 9th test case purposely has a '\0' in it -- don't count it
      // as the string terminator.
      url_len = 22;
    }
    url_parse::ParseMailtoURL(cases[i].input, url_len, &parsed);

    std::string out_str;
    url_canon::StdStringCanonOutput output(&out_str);
    bool success = url_canon::CanonicalizeMailtoURL(cases[i].input, url_len,
                                                    parsed, &output,
                                                    &out_parsed);
    output.Complete();

    EXPECT_EQ(cases[i].expected_success, success);
    EXPECT_EQ(cases[i].expected, out_str);

    // Make sure the spec was properly identified
    EXPECT_EQ(0, out_parsed.scheme.begin);
    EXPECT_EQ(6, out_parsed.scheme.len);

    EXPECT_EQ(cases[i].expected_path.begin, out_parsed.path.begin);
    EXPECT_EQ(cases[i].expected_path.len, out_parsed.path.len);

    EXPECT_EQ(cases[i].expected_query.begin, out_parsed.query.begin);
    EXPECT_EQ(cases[i].expected_query.len, out_parsed.query.len);
  }
}

#if !defined(WIN32) && !defined(__LB_XB1__) && !defined(__LB_XB360__)

TEST(URLCanonTest, _itoa_s) {
  // We fill the buffer with 0xff to ensure that it's getting properly
  // null-terminated.  We also allocate one byte more than what we tell
  // _itoa_s about, and ensure that the extra byte is untouched.
  char buf[6];
  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, url_canon::_itoa_s(12, buf, sizeof(buf) - 1, 10));
  EXPECT_STREQ("12", buf);
  EXPECT_EQ('\xFF', buf[3]);

  // Test the edge cases - exactly the buffer size and one over
  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, url_canon::_itoa_s(1234, buf, sizeof(buf) - 1, 10));
  EXPECT_STREQ("1234", buf);
  EXPECT_EQ('\xFF', buf[5]);

  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(EINVAL, url_canon::_itoa_s(12345, buf, sizeof(buf) - 1, 10));
  EXPECT_EQ('\xFF', buf[5]);  // should never write to this location

  // Test the template overload (note that this will see the full buffer)
  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, url_canon::_itoa_s(12, buf, 10));
  EXPECT_STREQ("12", buf);
  EXPECT_EQ('\xFF', buf[3]);

  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, url_canon::_itoa_s(12345, buf, 10));
  EXPECT_STREQ("12345", buf);

  EXPECT_EQ(EINVAL, url_canon::_itoa_s(123456, buf, 10));

  // Test that radix 16 is supported.
  memset(buf, 0xff, sizeof(buf));
  EXPECT_EQ(0, url_canon::_itoa_s(1234, buf, sizeof(buf) - 1, 16));
  EXPECT_STREQ("4d2", buf);
  EXPECT_EQ('\xFF', buf[5]);
}

TEST(URLCanonTest, _itow_s) {
  // We fill the buffer with 0xff to ensure that it's getting properly
  // null-terminated.  We also allocate one byte more than what we tell
  // _itoa_s about, and ensure that the extra byte is untouched.
  char16 buf[6];
  const char fill_mem = 0xff;
  const char16 fill_char = 0xffff;
  memset(buf, fill_mem, sizeof(buf));
  EXPECT_EQ(0, url_canon::_itow_s(12, buf, sizeof(buf) / 2 - 1, 10));
  EXPECT_EQ(WStringToUTF16(L"12"), string16(buf));
  EXPECT_EQ(fill_char, buf[3]);

  // Test the edge cases - exactly the buffer size and one over
  EXPECT_EQ(0, url_canon::_itow_s(1234, buf, sizeof(buf) / 2 - 1, 10));
  EXPECT_EQ(WStringToUTF16(L"1234"), string16(buf));
  EXPECT_EQ(fill_char, buf[5]);

  memset(buf, fill_mem, sizeof(buf));
  EXPECT_EQ(EINVAL, url_canon::_itow_s(12345, buf, sizeof(buf) / 2 - 1, 10));
  EXPECT_EQ(fill_char, buf[5]);  // should never write to this location

  // Test the template overload (note that this will see the full buffer)
  memset(buf, fill_mem, sizeof(buf));
  EXPECT_EQ(0, url_canon::_itow_s(12, buf, 10));
  EXPECT_EQ(WStringToUTF16(L"12"), string16(buf));
  EXPECT_EQ(fill_char, buf[3]);

  memset(buf, fill_mem, sizeof(buf));
  EXPECT_EQ(0, url_canon::_itow_s(12345, buf, 10));
  EXPECT_EQ(WStringToUTF16(L"12345"), string16(buf));

  EXPECT_EQ(EINVAL, url_canon::_itow_s(123456, buf, 10));
}

#endif  // !defined(WIN32) && !defined(__LB_XB1__) && !defined(__LB_XB360__)

// Returns true if the given two structures are the same.
static bool ParsedIsEqual(const url_parse::Parsed& a,
                          const url_parse::Parsed& b) {
  return a.scheme.begin == b.scheme.begin && a.scheme.len == b.scheme.len &&
         a.username.begin == b.username.begin && a.username.len == b.username.len &&
         a.password.begin == b.password.begin && a.password.len == b.password.len &&
         a.host.begin == b.host.begin && a.host.len == b.host.len &&
         a.port.begin == b.port.begin && a.port.len == b.port.len &&
         a.path.begin == b.path.begin && a.path.len == b.path.len &&
         a.query.begin == b.query.begin && a.query.len == b.query.len &&
         a.ref.begin == b.ref.begin && a.ref.len == b.ref.len;
}

TEST(URLCanonTest, ResolveRelativeURL) {
  struct RelativeCase {
    const char* base;      // Input base URL: MUST BE CANONICAL
    bool is_base_hier;     // Is the base URL hierarchical
    bool is_base_file;     // Tells us if the base is a file URL.
    const char* test;      // Input URL to test against.
    bool succeed_relative; // Whether we expect IsRelativeURL to succeed
    bool is_rel;           // Whether we expect |test| to be relative or not.
    bool succeed_resolve;  // Whether we expect ResolveRelativeURL to succeed.
    const char* resolved;  // What we expect in the result when resolving.
  } rel_cases[] = {
      // Basic absolute input.
    {"http://host/a", true, false, "http://another/", true, false, false, NULL},
    {"http://host/a", true, false, "http:////another/", true, false, false, NULL},
      // Empty relative URLs should only remove the ref part of the URL,
      // leaving the rest unchanged.
    {"http://foo/bar", true, false, "", true, true, true, "http://foo/bar"},
    {"http://foo/bar#ref", true, false, "", true, true, true, "http://foo/bar"},
    {"http://foo/bar#", true, false, "", true, true, true, "http://foo/bar"},
      // Spaces at the ends of the relative path should be ignored.
    {"http://foo/bar", true, false, "  another  ", true, true, true, "http://foo/another"},
    {"http://foo/bar", true, false, "  .  ", true, true, true, "http://foo/"},
    {"http://foo/bar", true, false, " \t ", true, true, true, "http://foo/bar"},
      // Matching schemes without two slashes are treated as relative.
    {"http://host/a", true, false, "http:path", true, true, true, "http://host/path"},
    {"http://host/a/", true, false, "http:path", true, true, true, "http://host/a/path"},
    {"http://host/a", true, false, "http:/path", true, true, true, "http://host/path"},
    {"http://host/a", true, false, "HTTP:/path", true, true, true, "http://host/path"},
      // Nonmatching schemes are absolute.
    {"http://host/a", true, false, "https:host2", true, false, false, NULL},
    {"http://host/a", true, false, "htto:/host2", true, false, false, NULL},
      // Absolute path input
    {"http://host/a", true, false, "/b/c/d", true, true, true, "http://host/b/c/d"},
    {"http://host/a", true, false, "\\b\\c\\d", true, true, true, "http://host/b/c/d"},
    {"http://host/a", true, false, "/b/../c", true, true, true, "http://host/c"},
    {"http://host/a?b#c", true, false, "/b/../c", true, true, true, "http://host/c"},
    {"http://host/a", true, false, "\\b/../c?x#y", true, true, true, "http://host/c?x#y"},
    {"http://host/a?b#c", true, false, "/b/../c?x#y", true, true, true, "http://host/c?x#y"},
      // Relative path input
    {"http://host/a", true, false, "b", true, true, true, "http://host/b"},
    {"http://host/a", true, false, "bc/de", true, true, true, "http://host/bc/de"},
    {"http://host/a/", true, false, "bc/de?query#ref", true, true, true, "http://host/a/bc/de?query#ref"},
    {"http://host/a/", true, false, ".", true, true, true, "http://host/a/"},
    {"http://host/a/", true, false, "..", true, true, true, "http://host/"},
    {"http://host/a/", true, false, "./..", true, true, true, "http://host/"},
    {"http://host/a/", true, false, "../.", true, true, true, "http://host/"},
    {"http://host/a/", true, false, "././.", true, true, true, "http://host/a/"},
    {"http://host/a?query#ref", true, false, "../../../foo", true, true, true, "http://host/foo"},
      // Query input
    {"http://host/a", true, false, "?foo=bar", true, true, true, "http://host/a?foo=bar"},
    {"http://host/a?x=y#z", true, false, "?", true, true, true, "http://host/a?"},
    {"http://host/a?x=y#z", true, false, "?foo=bar#com", true, true, true, "http://host/a?foo=bar#com"},
      // Ref input
    {"http://host/a", true, false, "#ref", true, true, true, "http://host/a#ref"},
    {"http://host/a#b", true, false, "#", true, true, true, "http://host/a#"},
    {"http://host/a?foo=bar#hello", true, false, "#bye", true, true, true, "http://host/a?foo=bar#bye"},
      // Non-hierarchical base: no relative handling. Relative input should
      // error, and if a scheme is present, it should be treated as absolute.
    {"data:foobar", false, false, "baz.html", false, false, false, NULL},
    {"data:foobar", false, false, "data:baz", true, false, false, NULL},
    {"data:foobar", false, false, "data:/base", true, false, false, NULL},
      // Non-hierarchical base: absolute input should succeed.
    {"data:foobar", false, false, "http://host/", true, false, false, NULL},
    {"data:foobar", false, false, "http:host", true, false, false, NULL},
      // Invalid schemes should be treated as relative.
    {"http://foo/bar", true, false, "./asd:fgh", true, true, true, "http://foo/asd:fgh"},
    {"http://foo/bar", true, false, ":foo", true, true, true, "http://foo/:foo"},
    {"http://foo/bar", true, false, " hello world", true, true, true, "http://foo/hello%20world"},
    {"data:asdf", false, false, ":foo", false, false, false, NULL},
      // We should treat semicolons like any other character in URL resolving
    {"http://host/a", true, false, ";foo", true, true, true, "http://host/;foo"},
    {"http://host/a;", true, false, ";foo", true, true, true, "http://host/;foo"},
    {"http://host/a", true, false, ";/../bar", true, true, true, "http://host/bar"},
      // Relative URLs can also be written as "//foo/bar" which is relative to
      // the scheme. In this case, it would take the old scheme, so for http
      // the example would resolve to "http://foo/bar".
    {"http://host/a", true, false, "//another", true, true, true, "http://another/"},
    {"http://host/a", true, false, "//another/path?query#ref", true, true, true, "http://another/path?query#ref"},
    {"http://host/a", true, false, "///another/path", true, true, true, "http://another/path"},
    {"http://host/a", true, false, "//Another\\path", true, true, true, "http://another/path"},
    {"http://host/a", true, false, "//", true, true, false, "http:"},
      // IE will also allow one or the other to be a backslash to get the same
      // behavior.
    {"http://host/a", true, false, "\\/another/path", true, true, true, "http://another/path"},
    {"http://host/a", true, false, "/\\Another\\path", true, true, true, "http://another/path"},
#ifdef WIN32
      // Resolving against Windows file base URLs.
    {"file:///C:/foo", true, true, "http://host/", true, false, false, NULL},
    {"file:///C:/foo", true, true, "bar", true, true, true, "file:///C:/bar"},
    {"file:///C:/foo", true, true, "../../../bar.html", true, true, true, "file:///C:/bar.html"},
    {"file:///C:/foo", true, true, "/../bar.html", true, true, true, "file:///C:/bar.html"},
      // But two backslashes on Windows should be UNC so should be treated
      // as absolute.
    {"http://host/a", true, false, "\\\\another\\path", true, false, false, NULL},
      // IE doesn't support drive specs starting with two slashes. It fails
      // immediately and doesn't even try to load. We fix it up to either
      // an absolute path or UNC depending on what it looks like.
    {"file:///C:/something", true, true, "//c:/foo", true, true, true, "file:///C:/foo"},
    {"file:///C:/something", true, true, "//localhost/c:/foo", true, true, true, "file:///C:/foo"},
      // Windows drive specs should be allowed and treated as absolute.
    {"file:///C:/foo", true, true, "c:", true, false, false, NULL},
    {"file:///C:/foo", true, true, "c:/foo", true, false, false, NULL},
    {"http://host/a", true, false, "c:\\foo", true, false, false, NULL},
      // Relative paths with drive letters should be allowed when the base is
      // also a file.
    {"file:///C:/foo", true, true, "/z:/bar", true, true, true, "file:///Z:/bar"},
      // Treat absolute paths as being off of the drive.
    {"file:///C:/foo", true, true, "/bar", true, true, true, "file:///C:/bar"},
    {"file://localhost/C:/foo", true, true, "/bar", true, true, true, "file://localhost/C:/bar"},
    {"file:///C:/foo/com/", true, true, "/bar", true, true, true, "file:///C:/bar"},
      // On Windows, two slashes without a drive letter when the base is a file
      // means that the path is UNC.
    {"file:///C:/something", true, true, "//somehost/path", true, true, true, "file://somehost/path"},
    {"file:///C:/something", true, true, "/\\//somehost/path", true, true, true, "file://somehost/path"},
#else
      // On Unix we fall back to relative behavior since there's nothing else
      // reasonable to do.
    {"http://host/a", true, false, "\\\\Another\\path", true, true, true, "http://another/path"},
#endif
      // Even on Windows, we don't allow relative drive specs when the base
      // is not file.
    {"http://host/a", true, false, "/c:\\foo", true, true, true, "http://host/c:/foo"},
    {"http://host/a", true, false, "//c:\\foo", true, true, true, "http://c/foo"},
      // Filesystem URL tests; filesystem URLs are only valid and relative if
      // they have no scheme, e.g. "./index.html".  There's no valid equivalent
      // to http:index.html.
    {"filesystem:http://host/t/path", true, false, "filesystem:http://host/t/path2", true, false, false, NULL},
    {"filesystem:http://host/t/path", true, false, "filesystem:https://host/t/path2", true, false, false, NULL},
    {"filesystem:http://host/t/path", true, false, "http://host/t/path2", true, false, false, NULL},
    {"http://host/t/path", true, false, "filesystem:http://host/t/path2", true, false, false, NULL},
    {"filesystem:http://host/t/path", true, false, "./path2", true, true, true, "filesystem:http://host/t/path2"},
    {"filesystem:http://host/t/path/", true, false, "path2", true, true, true, "filesystem:http://host/t/path/path2"},
    {"filesystem:http://host/t/path", true, false, "filesystem:http:path2", true, false, false, NULL},
  };

  for (size_t i = 0; i < ARRAYSIZE(rel_cases); i++) {
    const RelativeCase& cur_case = rel_cases[i];

    url_parse::Parsed parsed;
    int base_len = static_cast<int>(strlen(cur_case.base));
    if (cur_case.is_base_file)
      url_parse::ParseFileURL(cur_case.base, base_len, &parsed);
    else if (cur_case.is_base_hier)
      url_parse::ParseStandardURL(cur_case.base, base_len, &parsed);
    else
      url_parse::ParsePathURL(cur_case.base, base_len, &parsed);

    // First see if it is relative.
    int test_len = static_cast<int>(strlen(cur_case.test));
    bool is_relative;
    url_parse::Component relative_component;
    bool succeed_is_rel = url_canon::IsRelativeURL(
        cur_case.base, parsed, cur_case.test, test_len, cur_case.is_base_hier,
        &is_relative, &relative_component);

    EXPECT_EQ(cur_case.succeed_relative, succeed_is_rel) <<
        "succeed is rel failure on " << cur_case.test;
    EXPECT_EQ(cur_case.is_rel, is_relative) <<
        "is rel failure on " << cur_case.test;
    // Now resolve it.
    if (succeed_is_rel && is_relative && cur_case.is_rel) {
      std::string resolved;
      url_canon::StdStringCanonOutput output(&resolved);
      url_parse::Parsed resolved_parsed;

      bool succeed_resolve = url_canon::ResolveRelativeURL(
          cur_case.base, parsed, cur_case.is_base_file,
          cur_case.test, relative_component, NULL, &output, &resolved_parsed);
      output.Complete();

      EXPECT_EQ(cur_case.succeed_resolve, succeed_resolve);
      EXPECT_EQ(cur_case.resolved, resolved) << " on " << cur_case.test;

      // Verify that the output parsed structure is the same as parsing a
      // the URL freshly.
      url_parse::Parsed ref_parsed;
      int resolved_len = static_cast<int>(resolved.size());
      if (cur_case.is_base_file)
        url_parse::ParseFileURL(resolved.c_str(), resolved_len, &ref_parsed);
      else if (cur_case.is_base_hier)
        url_parse::ParseStandardURL(resolved.c_str(), resolved_len, &ref_parsed);
      else
        url_parse::ParsePathURL(resolved.c_str(), resolved_len, &ref_parsed);
      EXPECT_TRUE(ParsedIsEqual(ref_parsed, resolved_parsed));
    }
  }
}

// It used to be when we did a replacement with a long buffer of UTF-16
// characters, we would get invalid data in the URL. This is because the buffer
// it used to hold the UTF-8 data was resized, while some pointers were still
// kept to the old buffer that was removed.
TEST(URLCanonTest, ReplacementOverflow) {
  const char src[] = "file:///C:/foo/bar";
  int src_len = static_cast<int>(strlen(src));
  url_parse::Parsed parsed;
  url_parse::ParseFileURL(src, src_len, &parsed);

  // Override two components, the path with something short, and the query with
  // sonething long enough to trigger the bug.
  url_canon::Replacements<char16> repl;
  string16 new_query;
  for (int i = 0; i < 4800; i++)
    new_query.push_back('a');

  string16 new_path(WStringToUTF16(L"/foo"));
  repl.SetPath(new_path.c_str(), url_parse::Component(0, 4));
  repl.SetQuery(new_query.c_str(),
                url_parse::Component(0, static_cast<int>(new_query.length())));

  // Call ReplaceComponents on the string. It doesn't matter if we call it for
  // standard URLs, file URLs, etc, since they will go to the same replacement
  // function that was buggy.
  url_parse::Parsed repl_parsed;
  std::string repl_str;
  url_canon::StdStringCanonOutput repl_output(&repl_str);
  url_canon::ReplaceFileURL(src, parsed, repl, NULL, &repl_output, &repl_parsed);
  repl_output.Complete();

  // Generate the expected string and check.
  std::string expected("file:///foo?");
  for (size_t i = 0; i < new_query.length(); i++)
    expected.push_back('a');
  EXPECT_TRUE(expected == repl_str);
}

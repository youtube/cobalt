// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/email_input_type.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/script_regexp.h"

namespace blink {

namespace {

void ExpectToSucceed(const String& source) {
  ScriptRegexp* email_regexp = EmailInputType::CreateEmailRegexp();
  String result =
      EmailInputType::ConvertEmailAddressToASCII(*email_regexp, source);
  EXPECT_NE(source, result);
  EXPECT_TRUE(EmailInputType::IsValidEmailAddress(*email_regexp, result));
}

void ExpectToFail(const String& source) {
  ScriptRegexp* email_regexp = EmailInputType::CreateEmailRegexp();
  // Conversion failed.  The resultant value might contains non-ASCII
  // characters, and not a valid email address.
  EXPECT_FALSE(EmailInputType::IsValidEmailAddress(
      *email_regexp,
      EmailInputType::ConvertEmailAddressToASCII(*email_regexp, source)));
}

}  // namespace

TEST(EmailInputTypeTest, ConvertEmailAddressToASCII) {
  // U+043C U+043E U+0439 . U+0434 U+043E U+043C U+0435 U+043D
  ExpectToFail(
      String::FromUTF8("user@\xD0\xBC\xD0\xBE\xD0\xB9."
                       "\xD0\xB4\xD0\xBE\xD0\xBC\xD0\xB5\xD0\xBD@"));
  ExpectToFail(
      String::FromUTF8("user@\xD0\xBC\xD0\xBE\xD0\xB9. "
                       "\xD0\xB4\xD0\xBE\xD0\xBC\xD0\xB5\xD0\xBD"));
  ExpectToFail(
      String::FromUTF8("user@\xD0\xBC\xD0\xBE\xD0\xB9."
                       "\t\xD0\xB4\xD0\xBE\xD0\xBC\xD0\xB5\xD0\xBD"));
}

TEST(EmailInputTypeTest, ConvertEmailAddressToASCIIUTS46) {
  // http://unicode.org/reports/tr46/#Table_IDNA_Comparisons

  // U+00E0
  ExpectToSucceed(String::FromUTF8("foo@\xC3\xA0.com"));
  // U+FF01
  ExpectToFail(String::FromUTF8("foo@\xEF\xBC\x81.com"));

  // U+2132
  ExpectToFail(String::FromUTF8("foo@\xE2\x84\xB2.com"));
  // U+2F868
  ExpectToFail(String::FromUTF8("foo@\xF0\xAF\xA1\xA8.com"));

  // U+00C0
  ExpectToSucceed(String::FromUTF8("foo@\xC3\x80.com"));
  // U+2665
  ExpectToSucceed(String::FromUTF8("foo@\xE2\x99\xA5.com"));
  // U+00DF
  ExpectToSucceed(String::FromUTF8("foo@\xC3\x9F.com"));

  // U+0221
  ExpectToSucceed(String::FromUTF8("foo@\xC8\xA1.com"));
  // U+0662
  ExpectToFail(String::FromUTF8("foo@\xD8\x82.com"));

  // U+2615
  ExpectToSucceed(String::FromUTF8("foo@\xE2\x98\x95.com"));
  // U+023A
  ExpectToSucceed(String::FromUTF8("foo@\xC8\xBA.com"));
}

}  // namespace blink

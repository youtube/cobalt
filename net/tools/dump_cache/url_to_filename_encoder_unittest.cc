// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/dump_cache/url_to_filename_encoder.h"

#include <string>
#include <vector>
#include "base/string_piece.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::string;

namespace net {

// The escape character choice is made here -- all code and tests in this
// directory are based off of this constant.  However, our test ata
// has tons of dependencies on this, so it cannot be changed without
// re-running those tests and fixing them.
const char kTruncationChar = '-';
const char kEscapeChar = ',';
const size_t kMaximumSubdirectoryLength = 128;

class UrlToFilenameEncoderTest : public ::testing::Test {
 protected:
  UrlToFilenameEncoderTest() : escape_(1, kEscapeChar) {}

  void CheckSegmentLength(const StringPiece& escaped_word) {
    std::vector<StringPiece> components;
    Tokenize(escaped_word, StringPiece("/"), &components);
    for (size_t i = 0; i < components.size(); ++i) {
      EXPECT_GE(kMaximumSubdirectoryLength,
                components[i].size());
    }
  }

  void CheckValidChars(const StringPiece& escaped_word) {
    // These characters are invalid in Windows.  We will
    // ignore / for this test, but add in ', as that's pretty
    // inconvenient in a Unix filename.
    //
    // See http://msdn.microsoft.com/en-us/library/aa365247(VS.85).aspx
    static const char kInvalidChars[] = "<>:\"\\|?*'";
    for (size_t i = 0; i < escaped_word.size(); ++i) {
      char c = escaped_word[i];
      EXPECT_EQ(NULL, strchr(kInvalidChars, c));
      EXPECT_NE('\0', c);  // only invalid character in Posix
      EXPECT_GT(0x7E, c);  // only English printable characters
    }
  }

  void Validate(const string& in_word, const string& gold_word) {
    string escaped_word, url;
    UrlToFilenameEncoder::EncodeSegment("", in_word, '/', &escaped_word);
    EXPECT_EQ(gold_word, escaped_word);
    CheckSegmentLength(escaped_word);
    CheckValidChars(escaped_word);
    UrlToFilenameEncoder::Decode(escaped_word, '/', &url);
    EXPECT_EQ(in_word, url);
  }

  void ValidateAllSegmentsSmall(const string& in_word) {
    string escaped_word, url;
    UrlToFilenameEncoder::EncodeSegment("", in_word, '/', &escaped_word);
    CheckSegmentLength(escaped_word);
    CheckValidChars(escaped_word);
    UrlToFilenameEncoder::Decode(escaped_word, '/', &url);
    EXPECT_EQ(in_word, url);
  }

  void ValidateNoChange(const string& word) {
    // We always suffix the leaf with kEscapeChar, unless the leaf is empty.
    Validate(word, word + escape_);
  }

  void ValidateEscaped(unsigned char ch) {
    // We always suffix the leaf with kEscapeChar, unless the leaf is empty.
    char escaped[100];
    const char escape = kEscapeChar;
    base::snprintf(escaped, sizeof(escaped), "%c%02X%c", escape, ch, escape);
    Validate(string(1, ch), escaped);
  }

  string escape_;
};

TEST_F(UrlToFilenameEncoderTest, DoesNotEscape) {
  ValidateNoChange("");
  ValidateNoChange("abcdefg");
  ValidateNoChange("abcdefghijklmnopqrstuvwxyz");
  ValidateNoChange("ZYXWVUT");
  ValidateNoChange("ZYXWVUTSRQPONMLKJIHGFEDCBA");
  ValidateNoChange("01234567689");
  ValidateNoChange("/-_");
  ValidateNoChange("abcdefghijklmnopqrstuvwxyzZYXWVUTSRQPONMLKJIHGFEDCBA"
                   "01234567689/-_");
  ValidateNoChange("index.html");
  ValidateNoChange("/");
  ValidateNoChange("/.");
  ValidateNoChange(".");
  ValidateNoChange("..");
  ValidateNoChange("%");
  ValidateNoChange("=");
  ValidateNoChange("+");
  ValidateNoChange("_");
}

TEST_F(UrlToFilenameEncoderTest, Escapes) {
  ValidateEscaped('!');
  ValidateEscaped('"');
  ValidateEscaped('#');
  ValidateEscaped('$');
  ValidateEscaped('&');
  ValidateEscaped('(');
  ValidateEscaped(')');
  ValidateEscaped('*');
  ValidateEscaped(',');
  ValidateEscaped(':');
  ValidateEscaped(';');
  ValidateEscaped('<');
  ValidateEscaped('>');
  ValidateEscaped('@');
  ValidateEscaped('[');
  ValidateEscaped('\'');
  ValidateEscaped('\\');
  ValidateEscaped(']');
  ValidateEscaped('^');
  ValidateEscaped('`');
  ValidateEscaped('{');
  ValidateEscaped('|');
  ValidateEscaped('}');
  ValidateEscaped('~');

  // check non-printable characters
  ValidateEscaped('\0');
  for (int i = 127; i < 256; ++i) {
    ValidateEscaped(static_cast<char>(i));
  }
}

TEST_F(UrlToFilenameEncoderTest, DoesEscapeCorrectly) {
  Validate("mysite.com&x", "mysite.com" + escape_ + "26x" + escape_);
  Validate("/./", "/" + escape_ + "./" + escape_);
  Validate("/../", "/" + escape_ + "../" + escape_);
  Validate("//", "/" + escape_ + "/" + escape_);
  Validate("/./leaf", "/" + escape_ + "./leaf" + escape_);
  Validate("/../leaf", "/" + escape_ + "../leaf" + escape_);
  Validate("//leaf", "/" + escape_ + "/leaf" + escape_);
  Validate("mysite/u?param1=x&param2=y",
           "mysite/u" + escape_ + "3Fparam1=x" + escape_ + "26param2=y" +
           escape_);
  Validate("search?q=dogs&go=&form=QBLH&qs=n",  // from Latency Labs bing test.
           "search" + escape_ + "3Fq=dogs" + escape_ + "26go=" + escape_ +
           "26form=QBLH" + escape_ + "26qs=n" + escape_);
  Validate("~joebob/my_neeto-website+with_stuff.asp?id=138&content=true",
           "" + escape_ + "7Ejoebob/my_neeto-website+with_stuff.asp" + escape_ +
           "3Fid=138" + escape_ + "26content=true" + escape_);
}

TEST_F(UrlToFilenameEncoderTest, LongTail) {
  static char long_word[] =
      "~joebob/briggs/12345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890"
      "1234567890123456789012345678901234567890123456789012345678901234567890";

  // the long lines in the string below are 64 characters, so we can see
  // the slashes every 128.
  string gold_long_word =
      escape_ + "7Ejoebob/briggs/"
      "1234567890123456789012345678901234567890123456789012345678901234"
      "56789012345678901234567890123456789012345678901234567890123456" +
      escape_ + "-/"
      "7890123456789012345678901234567890123456789012345678901234567890"
      "12345678901234567890123456789012345678901234567890123456789012" +
      escape_ + "-/"
      "3456789012345678901234567890123456789012345678901234567890123456"
      "78901234567890123456789012345678901234567890123456789012345678" +
      escape_ + "-/"
      "9012345678901234567890" + escape_;
  EXPECT_LT(kMaximumSubdirectoryLength,
            sizeof(long_word));
  Validate(long_word, gold_long_word);
}

TEST_F(UrlToFilenameEncoderTest, LongTailQuestion) {
  // Here the '?' in the last path segment expands to @3F, making
  // it hit 128 chars before the input segment gets that big.
  static char long_word[] =
      "~joebob/briggs/1234567?1234567?1234567?1234567?1234567?"
      "1234567?1234567?1234567?1234567?1234567?1234567?1234567?"
      "1234567?1234567?1234567?1234567?1234567?1234567?1234567?"
      "1234567?1234567?1234567?1234567?1234567?1234567?1234567?"
      "1234567?1234567?1234567?1234567?1234567?1234567?1234567?"
      "1234567?1234567?1234567?1234567?1234567?1234567?1234567?";

  // Notice that at the end of the third segment, we avoid splitting
  // the (escape_ + "3F") that was generated from the "?", so that segment is
  // only 127 characters.
  string pattern = "1234567" + escape_ + "3F";  // 10 characters
  string gold_long_word =
      escape_ + "7Ejoebob/briggs/" +
      pattern + pattern + pattern + pattern + pattern + pattern + "1234"
      "567" + escape_ + "3F" + pattern + pattern + pattern + pattern + pattern +
       "123456" + escape_ + "-/"
      "7" + escape_ + "3F" + pattern + pattern + pattern + pattern + pattern +
      pattern + pattern + pattern + pattern + pattern + pattern + pattern +
      "12" +
      escape_ + "-/"
      "34567" + escape_ + "3F" + pattern + pattern + pattern + pattern + pattern
      + "1234567" + escape_ + "3F" + pattern + pattern + pattern + pattern
      + pattern + "1234567" +
      escape_ + "-/" +
      escape_ + "3F" + pattern + pattern + escape_;
  EXPECT_LT(kMaximumSubdirectoryLength,
            sizeof(long_word));
  Validate(long_word, gold_long_word);
}

TEST_F(UrlToFilenameEncoderTest, CornerCasesNearMaxLenNoEscape) {
  // hit corner cases, +/- 4 characters from kMaxLen
  for (int i = -4; i <= 4; ++i) {
    string input;
    input.append(i + kMaximumSubdirectoryLength, 'x');
    ValidateAllSegmentsSmall(input);
  }
}

TEST_F(UrlToFilenameEncoderTest, CornerCasesNearMaxLenWithEscape) {
  // hit corner cases, +/- 4 characters from kMaxLen.  This time we
  // leave off the last 'x' and put in a '.', which ensures that we
  // are truncating with '/' *after* the expansion.
  for (int i = -4; i <= 4; ++i) {
    string input;
    input.append(i + kMaximumSubdirectoryLength - 1, 'x');
    input.append(1, '.');  // this will expand to 3 characters.
    ValidateAllSegmentsSmall(input);
  }
}

TEST_F(UrlToFilenameEncoderTest, LeafBranchAlias) {
  Validate("/a/b/c", "/a/b/c" + escape_);        // c is leaf file "c,"
  Validate("/a/b/c/d", "/a/b/c/d" + escape_);    // c is directory "c"
  Validate("/a/b/c/d/", "/a/b/c/d/" + escape_);
}


TEST_F(UrlToFilenameEncoderTest, BackslashSeparator) {
  string long_word;
  string escaped_word;
  long_word.append(kMaximumSubdirectoryLength + 1, 'x');
  UrlToFilenameEncoder::EncodeSegment("", long_word, '\\', &escaped_word);

  // check that one backslash, plus the escape ",-", and the ending , got added.
  EXPECT_EQ(long_word.size() + 4, escaped_word.size());
  ASSERT_LT(kMaximumSubdirectoryLength,
            escaped_word.size());
  // Check that the backslash got inserted at the correct spot.
  EXPECT_EQ('\\', escaped_word[
      kMaximumSubdirectoryLength]);
}

}  // namespace


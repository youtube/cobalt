// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "net/cookies/parsed_cookie.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class ParsedCookieTest : public testing::Test { };

}  // namespace

TEST(ParsedCookieTest, TestBasic) {
  ParsedCookie pc("a=b");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_FALSE(pc.IsSecure());
  EXPECT_EQ("a", pc.Name());
  EXPECT_EQ("b", pc.Value());
}

TEST(ParsedCookieTest, TestQuoted) {
  // These are some quoting cases which the major browsers all
  // handle differently.  I've tested Internet Explorer 6, Opera 9.6,
  // Firefox 3, and Safari Windows 3.2.1.  We originally tried to match
  // Firefox closely, however we now match Internet Explorer and Safari.
  const char* values[] = {
    // Trailing whitespace after a quoted value.  The whitespace after
    // the quote is stripped in all browsers.
    "\"zzz \"  ",              "\"zzz \"",
    // Handling a quoted value with a ';', like FOO="zz;pp"  ;
    // IE and Safari: "zz;
    // Firefox and Opera: "zz;pp"
    "\"zz;pp\" ;",             "\"zz",
    // Handling a value with multiple quoted parts, like FOO="zzz "   "ppp" ;
    // IE and Safari: "zzz "   "ppp";
    // Firefox: "zzz ";
    // Opera: <rejects cookie>
    "\"zzz \"   \"ppp\" ",     "\"zzz \"   \"ppp\"",
    // A quote in a value that didn't start quoted.  like FOO=A"B ;
    // IE, Safari, and Firefox: A"B;
    // Opera: <rejects cookie>
    "A\"B",                    "A\"B",
  };

  for (size_t i = 0; i < arraysize(values); i += 2) {
    std::string input(values[i]);
    std::string expected(values[i + 1]);

    ParsedCookie pc("aBc=" + input + " ; path=\"/\"  ; httponly ");
    EXPECT_TRUE(pc.IsValid());
    EXPECT_FALSE(pc.IsSecure());
    EXPECT_TRUE(pc.IsHttpOnly());
    EXPECT_TRUE(pc.HasPath());
    EXPECT_EQ("aBc", pc.Name());
    EXPECT_EQ(expected, pc.Value());

    // If a path was quoted, the path attribute keeps the quotes.  This will
    // make the cookie effectively useless, but path parameters aren't supposed
    // to be quoted.  Bug 1261605.
    EXPECT_EQ("\"/\"", pc.Path());
  }
}

TEST(ParsedCookieTest, TestNameless) {
  ParsedCookie pc("BLAHHH; path=/; secure;");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("BLAHHH", pc.Value());
}

TEST(ParsedCookieTest, TestAttributeCase) {
  ParsedCookie pc("BLAHHH; Path=/; sECuRe; httpONLY");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("BLAHHH", pc.Value());
  EXPECT_EQ(3U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TestDoubleQuotedNameless) {
  ParsedCookie pc("\"BLA\\\"HHH\"; path=/; secure;");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("\"BLA\\\"HHH\"", pc.Value());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, QuoteOffTheEnd) {
  ParsedCookie pc("a=\"B");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("a", pc.Name());
  EXPECT_EQ("\"B", pc.Value());
  EXPECT_EQ(0U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MissingName) {
  ParsedCookie pc("=ABC");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("", pc.Name());
  EXPECT_EQ("ABC", pc.Value());
  EXPECT_EQ(0U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MissingValue) {
  ParsedCookie pc("ABC=;  path = /wee");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ABC", pc.Name());
  EXPECT_EQ("", pc.Value());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/wee", pc.Path());
  EXPECT_EQ(1U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, Whitespace) {
  ParsedCookie pc("  A  = BC  ;secure;;;   httponly");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("A", pc.Name());
  EXPECT_EQ("BC", pc.Value());
  EXPECT_FALSE(pc.HasPath());
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  // We parse anything between ; as attributes, so we end up with two
  // attributes with an empty string name and value.
  EXPECT_EQ(4U, pc.NumberOfAttributes());
}
TEST(ParsedCookieTest, MultipleEquals) {
  ParsedCookie pc("  A=== BC  ;secure;;;   httponly");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("A", pc.Name());
  EXPECT_EQ("== BC", pc.Value());
  EXPECT_FALSE(pc.HasPath());
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  EXPECT_EQ(4U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MACKey) {
  ParsedCookie pc("foo=bar; MAC-Key=3900ac9anw9incvw9f");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("foo", pc.Name());
  EXPECT_EQ("bar", pc.Value());
  EXPECT_EQ("3900ac9anw9incvw9f", pc.MACKey());
  EXPECT_EQ(1U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MACAlgorithm) {
  ParsedCookie pc("foo=bar; MAC-Algorithm=hmac-sha-1");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("foo", pc.Name());
  EXPECT_EQ("bar", pc.Value());
  EXPECT_EQ("hmac-sha-1", pc.MACAlgorithm());
  EXPECT_EQ(1U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, MACKeyAndMACAlgorithm) {
  ParsedCookie pc(
        "foo=bar; MAC-Key=voiae-09fj0302nfqf; MAC-Algorithm=hmac-sha-256");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("foo", pc.Name());
  EXPECT_EQ("bar", pc.Value());
  EXPECT_EQ("voiae-09fj0302nfqf", pc.MACKey());
  EXPECT_EQ("hmac-sha-256", pc.MACAlgorithm());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, QuotedTrailingWhitespace) {
  ParsedCookie pc("ANCUUID=\"zohNumRKgI0oxyhSsV3Z7D\"  ; "
                      "expires=Sun, 18-Apr-2027 21:06:29 GMT ; "
                      "path=/  ;  ");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ANCUUID", pc.Name());
  // Stripping whitespace after the quotes matches all other major browsers.
  EXPECT_EQ("\"zohNumRKgI0oxyhSsV3Z7D\"", pc.Value());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TrailingWhitespace) {
  ParsedCookie pc("ANCUUID=zohNumRKgI0oxyhSsV3Z7D  ; "
                      "expires=Sun, 18-Apr-2027 21:06:29 GMT ; "
                      "path=/  ;  ");
  EXPECT_TRUE(pc.IsValid());
  EXPECT_EQ("ANCUUID", pc.Name());
  EXPECT_EQ("zohNumRKgI0oxyhSsV3Z7D", pc.Value());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_EQ("/", pc.Path());
  EXPECT_EQ(2U, pc.NumberOfAttributes());
}

TEST(ParsedCookieTest, TooManyPairs) {
  std::string blankpairs;
  blankpairs.resize(ParsedCookie::kMaxPairs - 1, ';');

  ParsedCookie pc1(blankpairs + "secure");
  EXPECT_TRUE(pc1.IsValid());
  EXPECT_TRUE(pc1.IsSecure());

  ParsedCookie pc2(blankpairs + ";secure");
  EXPECT_TRUE(pc2.IsValid());
  EXPECT_FALSE(pc2.IsSecure());
}

// TODO(erikwright): some better test cases for invalid cookies.
TEST(ParsedCookieTest, InvalidWhitespace) {
  ParsedCookie pc("    ");
  EXPECT_FALSE(pc.IsValid());
}

TEST(ParsedCookieTest, InvalidTooLong) {
  std::string maxstr;
  maxstr.resize(ParsedCookie::kMaxCookieSize, 'a');

  ParsedCookie pc1(maxstr);
  EXPECT_TRUE(pc1.IsValid());

  ParsedCookie pc2(maxstr + "A");
  EXPECT_FALSE(pc2.IsValid());
}

TEST(ParsedCookieTest, InvalidEmpty) {
  ParsedCookie pc("");
  EXPECT_FALSE(pc.IsValid());
}

TEST(ParsedCookieTest, EmbeddedTerminator) {
  ParsedCookie pc1("AAA=BB\0ZYX");
  ParsedCookie pc2("AAA=BB\rZYX");
  ParsedCookie pc3("AAA=BB\nZYX");
  EXPECT_TRUE(pc1.IsValid());
  EXPECT_EQ("AAA", pc1.Name());
  EXPECT_EQ("BB", pc1.Value());
  EXPECT_TRUE(pc2.IsValid());
  EXPECT_EQ("AAA", pc2.Name());
  EXPECT_EQ("BB", pc2.Value());
  EXPECT_TRUE(pc3.IsValid());
  EXPECT_EQ("AAA", pc3.Name());
  EXPECT_EQ("BB", pc3.Value());
}

TEST(ParsedCookieTest, ParseTokensAndValues) {
  EXPECT_EQ("hello",
            ParsedCookie::ParseTokenString("hello\nworld"));
  EXPECT_EQ("fs!!@",
            ParsedCookie::ParseTokenString("fs!!@;helloworld"));
  EXPECT_EQ("hello world\tgood",
            ParsedCookie::ParseTokenString("hello world\tgood\rbye"));
  EXPECT_EQ("A",
            ParsedCookie::ParseTokenString("A=B=C;D=E"));
  EXPECT_EQ("hello",
            ParsedCookie::ParseValueString("hello\nworld"));
  EXPECT_EQ("fs!!@",
            ParsedCookie::ParseValueString("fs!!@;helloworld"));
  EXPECT_EQ("hello world\tgood",
            ParsedCookie::ParseValueString("hello world\tgood\rbye"));
  EXPECT_EQ("A=B=C",
            ParsedCookie::ParseValueString("A=B=C;D=E"));
}

TEST(ParsedCookieTest, SerializeCookieLine) {
  const char input[] = "ANCUUID=zohNumRKgI0oxyhSsV3Z7D  ; "
                       "expires=Sun, 18-Apr-2027 21:06:29 GMT ; "
                       "path=/  ;  ";
  const char output[] = "ANCUUID=zohNumRKgI0oxyhSsV3Z7D; "
                        "expires=Sun, 18-Apr-2027 21:06:29 GMT; "
                        "path=/";
  ParsedCookie pc(input);
  EXPECT_EQ(output, pc.ToCookieLine());
}


TEST(ParsedCookieTest, SetNameAndValue) {
  ParsedCookie empty("");
  EXPECT_FALSE(empty.IsValid());
  EXPECT_FALSE(empty.SetDomain("foobar.com"));
  EXPECT_TRUE(empty.SetName("name"));
  EXPECT_TRUE(empty.SetValue("value"));
  EXPECT_EQ("name=value", empty.ToCookieLine());
  EXPECT_TRUE(empty.IsValid());

  // We don't test
  //   ParsedCookie invalid("@foo=bar");
  //   EXPECT_FALSE(invalid.IsValid());
  // here because we are slightly more tolerant to invalid cookie names and
  // values that are set by webservers. We only enforce a correct name and
  // value if set via SetName() and SetValue().

  ParsedCookie pc("name=value");
  EXPECT_TRUE(pc.IsValid());

  // Set invalid name / value.
  EXPECT_FALSE(pc.SetName("@foobar"));
  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_FALSE(pc.SetName(""));
  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_FALSE(pc.SetValue("foo bar"));
  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_FALSE(pc.SetValue("\"foobar"));
  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  // Set valid name / value
  EXPECT_TRUE(pc.SetName("test"));
  EXPECT_EQ("test=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetValue("\"foobar\""));
  EXPECT_EQ("test=\"foobar\"", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  EXPECT_TRUE(pc.SetValue(""));
  EXPECT_EQ("test=", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());
}

TEST(ParsedCookieTest, SetAttributes) {
  ParsedCookie pc("name=value");
  EXPECT_TRUE(pc.IsValid());

  // Clear an unset attribute.
  EXPECT_TRUE(pc.SetDomain(""));
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  // Set a string containing an invalid character
  EXPECT_FALSE(pc.SetDomain("foo;bar"));
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_EQ("name=value", pc.ToCookieLine());
  EXPECT_TRUE(pc.IsValid());

  // Set all other attributes and check that they are appended in order.
  EXPECT_TRUE(pc.SetDomain("domain.com"));
  EXPECT_TRUE(pc.SetPath("/"));
  EXPECT_TRUE(pc.SetMACKey("mackey"));
  EXPECT_TRUE(pc.SetMACAlgorithm("\"macalgorithm\""));
  EXPECT_TRUE(pc.SetExpires("Sun, 18-Apr-2027 21:06:29 GMT"));
  EXPECT_TRUE(pc.SetMaxAge("12345"));
  EXPECT_TRUE(pc.SetIsSecure(true));
  EXPECT_TRUE(pc.SetIsHttpOnly(true));
  EXPECT_EQ("name=value; domain=domain.com; path=/; mac-key=mackey; "
            "mac-algorithm=\"macalgorithm\"; "
            "expires=Sun, 18-Apr-2027 21:06:29 GMT; max-age=12345; secure; "
            "httponly",
            pc.ToCookieLine());
  EXPECT_TRUE(pc.HasDomain());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_TRUE(pc.HasMACKey());
  EXPECT_TRUE(pc.HasMACAlgorithm());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.HasMaxAge());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());

  // Clear one attribute from the middle.
  EXPECT_TRUE(pc.SetMACAlgorithm(""));
  EXPECT_TRUE(pc.HasDomain());
  EXPECT_TRUE(pc.HasPath());
  EXPECT_TRUE(pc.HasMACKey());
  EXPECT_FALSE(pc.HasMACAlgorithm());
  EXPECT_TRUE(pc.HasExpires());
  EXPECT_TRUE(pc.HasMaxAge());
  EXPECT_TRUE(pc.IsSecure());
  EXPECT_TRUE(pc.IsHttpOnly());
  EXPECT_EQ("name=value; domain=domain.com; path=/; mac-key=mackey; "
            "expires=Sun, 18-Apr-2027 21:06:29 GMT; max-age=12345; secure; "
            "httponly",
            pc.ToCookieLine());

  // Clear the rest and change the name and value.
  EXPECT_TRUE(pc.SetDomain(""));
  EXPECT_TRUE(pc.SetPath(""));
  EXPECT_TRUE(pc.SetMACKey(""));
  EXPECT_TRUE(pc.SetMACAlgorithm(""));
  EXPECT_TRUE(pc.SetExpires(""));
  EXPECT_TRUE(pc.SetMaxAge(""));
  EXPECT_TRUE(pc.SetIsSecure(false));
  EXPECT_TRUE(pc.SetIsHttpOnly(false));
  EXPECT_TRUE(pc.SetName("name2"));
  EXPECT_TRUE(pc.SetValue("value2"));
  EXPECT_FALSE(pc.HasDomain());
  EXPECT_FALSE(pc.HasPath());
  EXPECT_FALSE(pc.HasMACKey());
  EXPECT_FALSE(pc.HasMACAlgorithm());
  EXPECT_FALSE(pc.HasExpires());
  EXPECT_FALSE(pc.HasMaxAge());
  EXPECT_FALSE(pc.IsSecure());
  EXPECT_FALSE(pc.IsHttpOnly());
  EXPECT_EQ("name2=value2", pc.ToCookieLine());
}

}  // namespace net

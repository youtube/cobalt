// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/basictypes.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using net::HttpUtil;

namespace {
class HttpUtilTest : public testing::Test {};
}

TEST(HttpUtilTest, HasHeader) {
  static const struct {
    const char* headers;
    const char* name;
    bool expected_result;
  } tests[] = {
    { "", "foo", false },
    { "foo\r\nbar", "foo", false },
    { "ffoo: 1", "foo", false },
    { "foo: 1", "foo", true },
    { "foo: 1\r\nbar: 2", "foo", true },
    { "fOO: 1\r\nbar: 2", "foo", true },
    { "g: 0\r\nfoo: 1\r\nbar: 2", "foo", true },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    bool result = HttpUtil::HasHeader(tests[i].headers, tests[i].name);
    EXPECT_EQ(tests[i].expected_result, result);
  }
}

TEST(HttpUtilTest, StripHeaders) {
  static const char* headers =
      "Origin: origin\r\n"
      "Content-Type: text/plain\r\n"
      "Cookies: foo1\r\n"
      "Custom: baz\r\n"
      "COOKIES: foo2\r\n"
      "Server: Apache\r\n"
      "OrIGin: origin2\r\n";

  static const char* header_names[] = {
    "origin", "content-type", "cookies"
  };

  static const char* expected_stripped_headers =
      "Custom: baz\r\n"
      "Server: Apache\r\n";

  EXPECT_EQ(expected_stripped_headers,
            HttpUtil::StripHeaders(headers, header_names,
                                   arraysize(header_names)));
}

TEST(HttpUtilTest, HeadersIterator) {
  std::string headers = "foo: 1\t\r\nbar: hello world\r\nbaz: 3 \r\n";

  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("foo"), it.name());
  EXPECT_EQ(std::string("1"), it.values());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("bar"), it.name());
  EXPECT_EQ(std::string("hello world"), it.values());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("baz"), it.name());
  EXPECT_EQ(std::string("3"), it.values());

  EXPECT_FALSE(it.GetNext());
}

TEST(HttpUtilTest, HeadersIterator_MalformedLine) {
  std::string headers = "foo: 1\n: 2\n3\nbar: 4";

  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\n");

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("foo"), it.name());
  EXPECT_EQ(std::string("1"), it.values());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("bar"), it.name());
  EXPECT_EQ(std::string("4"), it.values());

  EXPECT_FALSE(it.GetNext());
}

TEST(HttpUtilTest, HeadersIterator_AdvanceTo) {
  std::string headers = "foo: 1\r\n: 2\r\n3\r\nbar: 4";

  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");
  EXPECT_TRUE(it.AdvanceTo("foo"));
  EXPECT_EQ("foo", it.name());
  EXPECT_TRUE(it.AdvanceTo("bar"));
  EXPECT_EQ("bar", it.name());
  EXPECT_FALSE(it.AdvanceTo("blat"));
  EXPECT_FALSE(it.GetNext());  // should be at end of headers
}

TEST(HttpUtilTest, HeadersIterator_Reset) {
  std::string headers = "foo: 1\r\n: 2\r\n3\r\nbar: 4";
  HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\r\n");
  // Search past "foo".
  EXPECT_TRUE(it.AdvanceTo("bar"));
  // Now try advancing to "foo".  This time it should fail since the iterator
  // position is past it.
  EXPECT_FALSE(it.AdvanceTo("foo"));
  it.Reset();
  // Now that we reset the iterator position, we should find 'foo'
  EXPECT_TRUE(it.AdvanceTo("foo"));
}

TEST(HttpUtilTest, ValuesIterator) {
  std::string values = " must-revalidate,   no-cache=\"foo, bar\"\t, private ";

  HttpUtil::ValuesIterator it(values.begin(), values.end(), ',');

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("must-revalidate"), it.value());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("no-cache=\"foo, bar\""), it.value());

  ASSERT_TRUE(it.GetNext());
  EXPECT_EQ(std::string("private"), it.value());

  EXPECT_FALSE(it.GetNext());
}

TEST(HttpUtilTest, ValuesIterator_Blanks) {
  std::string values = " \t ";

  HttpUtil::ValuesIterator it(values.begin(), values.end(), ',');

  EXPECT_FALSE(it.GetNext());
}

TEST(HttpUtilTest, Unquote) {
  // Replace <backslash> " with ".
  EXPECT_STREQ("xyz\"abc", HttpUtil::Unquote("\"xyz\\\"abc\"").c_str());

  // Replace <backslash> <backslash> with <backslash>
  EXPECT_STREQ("xyz\\abc", HttpUtil::Unquote("\"xyz\\\\abc\"").c_str());
  EXPECT_STREQ("xyz\\\\\\abc",
               HttpUtil::Unquote("\"xyz\\\\\\\\\\\\abc\"").c_str());

  // Replace <backslash> X with X
  EXPECT_STREQ("xyzXabc", HttpUtil::Unquote("\"xyz\\Xabc\"").c_str());

  // Act as identity function on unquoted inputs.
  EXPECT_STREQ("X", HttpUtil::Unquote("X").c_str());
  EXPECT_STREQ("\"", HttpUtil::Unquote("\"").c_str());

  // Allow single quotes to act as quote marks.
  // Not part of RFC 2616.
  EXPECT_STREQ("x\"", HttpUtil::Unquote("'x\"'").c_str());
}

TEST(HttpUtilTest, Quote) {
  EXPECT_STREQ("\"xyz\\\"abc\"", HttpUtil::Quote("xyz\"abc").c_str());

  // Replace <backslash> <backslash> with <backslash>
  EXPECT_STREQ("\"xyz\\\\abc\"", HttpUtil::Quote("xyz\\abc").c_str());

  // Replace <backslash> X with X
  EXPECT_STREQ("\"xyzXabc\"", HttpUtil::Quote("xyzXabc").c_str());
}

TEST(HttpUtilTest, LocateEndOfHeaders) {
  struct {
    const char* input;
    int expected_result;
  } tests[] = {
    { "foo\r\nbar\r\n\r\n", 12 },
    { "foo\nbar\n\n", 9 },
    { "foo\r\nbar\r\n\r\njunk", 12 },
    { "foo\nbar\n\njunk", 9 },
    { "foo\nbar\n\r\njunk", 10 },
    { "foo\nbar\r\n\njunk", 10 },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    int input_len = static_cast<int>(strlen(tests[i].input));
    int eoh = HttpUtil::LocateEndOfHeaders(tests[i].input, input_len);
    EXPECT_EQ(tests[i].expected_result, eoh);
  }
}

TEST(HttpUtilTest, AssembleRawHeaders) {
  struct {
    const char* input;
    const char* expected_result;  // with '\0' changed to '|'
  } tests[] = {
    { "HTTP/1.0 200 OK\r\nFoo: 1\r\nBar: 2\r\n\r\n",
      "HTTP/1.0 200 OK|Foo: 1|Bar: 2||" },

    { "HTTP/1.0 200 OK\nFoo: 1\nBar: 2\n\n",
      "HTTP/1.0 200 OK|Foo: 1|Bar: 2||" },

    // Valid line continuation (single SP).
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      " continuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1 continuation|"
      "Bar: 2||"
    },

    // Valid line continuation (single HT).
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "\tcontinuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1 continuation|"
      "Bar: 2||"
    },

    // Valid line continuation (multiple SP).
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "   continuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1 continuation|"
      "Bar: 2||"
    },

    // Valid line continuation (multiple HT).
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "\t\t\tcontinuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1 continuation|"
      "Bar: 2||"
    },

    // Valid line continuation (mixed HT, SP).
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      " \t \t continuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1 continuation|"
      "Bar: 2||"
    },

    // Valid multi-line continuation
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      " continuation1\n"
      "\tcontinuation2\n"
      "  continuation3\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1 continuation1 continuation2 continuation3|"
      "Bar: 2||"
    },

    // Continuation of quoted value.
    // This is different from what Firefox does, since it
    // will preserve the LWS.
    {
      "HTTP/1.0 200 OK\n"
      "Etag: \"34534-d3\n"
      "    134q\"\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Etag: \"34534-d3 134q\"|"
      "Bar: 2||"
    },

    // Valid multi-line continuation, full LWS lines
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "         \n"
      "\t\t\t\t\n"
      "\t  continuation\n"
      "Bar: 2\n\n",

      // One SP per continued line = 3.
      "HTTP/1.0 200 OK|"
      "Foo: 1   continuation|"
      "Bar: 2||"
    },

    // Valid multi-line continuation, all LWS
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "         \n"
      "\t\t\t\t\n"
      "\t  \n"
      "Bar: 2\n\n",

      // One SP per continued line = 3.
      "HTTP/1.0 200 OK|"
      "Foo: 1   |"
      "Bar: 2||"
    },

    // Valid line continuation (No value bytes in first line).
    {
      "HTTP/1.0 200 OK\n"
      "Foo:\n"
      " value\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: value|"
      "Bar: 2||"
    },

    // Not a line continuation (can't continue status line).
    {
      "HTTP/1.0 200 OK\n"
      " Foo: 1\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      " Foo: 1|"
      "Bar: 2||"
    },

    // Not a line continuation (can't continue status line).
    {
      "HTTP/1.0\n"
      " 200 OK\n"
      "Foo: 1\n"
      "Bar: 2\n\n",

      "HTTP/1.0|"
      " 200 OK|"
      "Foo: 1|"
      "Bar: 2||"
    },

    // Not a line continuation (can't continue status line).
    {
      "HTTP/1.0 404\n"
      " Not Found\n"
      "Foo: 1\n"
      "Bar: 2\n\n",

      "HTTP/1.0 404|"
      " Not Found|"
      "Foo: 1|"
      "Bar: 2||"
    },

    // Unterminated status line.
    {
      "HTTP/1.0 200 OK",

      "HTTP/1.0 200 OK||"
    },

    // Single terminated, with headers
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "Bar: 2\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1|"
      "Bar: 2||"
    },

    // Not terminated, with headers
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "Bar: 2",

      "HTTP/1.0 200 OK|"
      "Foo: 1|"
      "Bar: 2||"
    },

    // Not a line continuation (VT)
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "\vInvalidContinuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1|"
      "\vInvalidContinuation|"
      "Bar: 2||"
    },

    // Not a line continuation (formfeed)
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "\fInvalidContinuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1|"
      "\fInvalidContinuation|"
      "Bar: 2||"
    },

    // Not a line continuation -- can't continue header names.
    {
      "HTTP/1.0 200 OK\n"
      "Serv\n"
      " er: Apache\n"
      "\tInvalidContinuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Serv|"
      " er: Apache|"
      "\tInvalidContinuation|"
      "Bar: 2||"
    },

    // Not a line continuation -- no value to continue.
    {
      "HTTP/1.0 200 OK\n"
      "Foo: 1\n"
      "garbage\n"
      "  not-a-continuation\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "Foo: 1|"
      "garbage|"
      "  not-a-continuation|"
      "Bar: 2||",
    },

    // Not a line continuation -- no valid name.
    {
      "HTTP/1.0 200 OK\n"
      ": 1\n"
      "  garbage\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      ": 1|"
      "  garbage|"
      "Bar: 2||",
    },

    // Not a line continuation -- no valid name (whitespace)
    {
      "HTTP/1.0 200 OK\n"
      "   : 1\n"
      "  garbage\n"
      "Bar: 2\n\n",

      "HTTP/1.0 200 OK|"
      "   : 1|"
      "  garbage|"
      "Bar: 2||",
    },

  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    int input_len = static_cast<int>(strlen(tests[i].input));
    std::string raw = HttpUtil::AssembleRawHeaders(tests[i].input, input_len);
    std::replace(raw.begin(), raw.end(), '\0', '|');
    EXPECT_TRUE(raw == tests[i].expected_result);
  }
}

// Test SpecForRequest() and PathForRequest().
TEST(HttpUtilTest, RequestUrlSanitize) {
  struct {
    const char* url;
    const char* expected_spec;
    const char* expected_path;
  } tests[] = {
    { // Check that #hash is removed.
      "http://www.google.com:78/foobar?query=1#hash",
      "http://www.google.com:78/foobar?query=1",
      "/foobar?query=1"
    },
    { // The reference may itself contain # -- strip all of it.
      "http://192.168.0.1?query=1#hash#10#11#13#14",
      "http://192.168.0.1/?query=1",
      "/?query=1"
    },
    { // Strip username/password.
      "http://user:pass@google.com",
      "http://google.com/",
      "/"
    }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    GURL url(GURL(tests[i].url));
    std::string expected_spec(tests[i].expected_spec);
    std::string expected_path(tests[i].expected_path);

    EXPECT_EQ(expected_spec, HttpUtil::SpecForRequest(url));
    EXPECT_EQ(expected_path, HttpUtil::PathForRequest(url));
  }
}

TEST(HttpUtilTest, GenerateAcceptLanguageHeader) {
  EXPECT_EQ(std::string("en-US,fr;q=0.8,de;q=0.6"),
            HttpUtil::GenerateAcceptLanguageHeader("en-US,fr,de"));
  EXPECT_EQ(std::string("en-US,fr;q=0.8,de;q=0.6,ko;q=0.4,zh-CN;q=0.2,"
                        "ja;q=0.2"),
            HttpUtil::GenerateAcceptLanguageHeader("en-US,fr,de,ko,zh-CN,ja"));
}

TEST(HttpUtilTest, GenerateAcceptCharsetHeader) {
  EXPECT_EQ(std::string("utf-8,*;q=0.5"),
            HttpUtil::GenerateAcceptCharsetHeader("utf-8"));
  EXPECT_EQ(std::string("EUC-JP,utf-8;q=0.7,*;q=0.3"),
            HttpUtil::GenerateAcceptCharsetHeader("EUC-JP"));
}

TEST(HttpUtilTest, ParseRanges) {
  const struct {
    const char* headers;
    bool expected_return_value;
    size_t expected_ranges_size;
    const struct {
      int64 expected_first_byte_position;
      int64 expected_last_byte_position;
      int64 expected_suffix_length;
    } expected_ranges[10];
  } tests[] = {
    { "Range: bytes=0-10",
      true,
      1,
      { {0, 10, -1}, }
    },
    { "Range: bytes=10-0",
      false,
      0,
      {}
    },
    { "Range: BytES=0-10",
      true,
      1,
      { {0, 10, -1}, }
    },
    { "Range: megabytes=0-10",
      false,
      0,
      {}
    },
    { "Range: bytes0-10",
      false,
      0,
      {}
    },
    { "Range: bytes=0-0,0-10,10-20,100-200,100-,-200",
      true,
      6,
      { {0, 0, -1},
        {0, 10, -1},
        {10, 20, -1},
        {100, 200, -1},
        {100, -1, -1},
        {-1, -1, 200},
      }
    },
    { "Range: bytes=0-10\r\n"
      "Range: bytes=0-10,10-20,100-200,100-,-200",
      true,
      1,
      { {0, 10, -1}
      }
    },
    { "Range: bytes=",
      false,
      0,
      {}
    },
    { "Range: bytes=-",
      false,
      0,
      {}
    },
    { "Range: bytes=0-10-",
      false,
      0,
      {}
    },
    { "Range: bytes=-0-10",
      false,
      0,
      {}
    },
    { "Range: bytes =0-10\r\n",
      true,
      1,
      { {0, 10, -1}
      }
    },
    { "Range: bytes=  0-10      \r\n",
      true,
      1,
      { {0, 10, -1}
      }
    },
    { "Range: bytes  =   0  -   10      \r\n",
      true,
      1,
      { {0, 10, -1}
      }
    },
    { "Range: bytes=   0-1   0\r\n",
      false,
      0,
      {}
    },
    { "Range: bytes=   0-     -10\r\n",
      false,
      0,
      {}
    },
    { "Range: bytes=   0  -  1   ,   10 -20,   100- 200 ,  100-,  -200 \r\n",
      true,
      5,
      { {0, 1, -1},
        {10, 20, -1},
        {100, 200, -1},
        {100, -1, -1},
        {-1, -1, 200},
      }
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::vector<net::HttpByteRange> ranges;
    bool return_value = HttpUtil::ParseRanges(std::string(tests[i].headers),
                                              &ranges);
    EXPECT_EQ(tests[i].expected_return_value, return_value);
    if (return_value) {
      EXPECT_EQ(tests[i].expected_ranges_size, ranges.size());
      for (size_t j = 0; j < ranges.size(); ++j) {
        EXPECT_EQ(tests[i].expected_ranges[j].expected_first_byte_position,
                  ranges[j].first_byte_position());
        EXPECT_EQ(tests[i].expected_ranges[j].expected_last_byte_position,
                  ranges[j].last_byte_position());
        EXPECT_EQ(tests[i].expected_ranges[j].expected_suffix_length,
                  ranges[j].suffix_length());
      }
    }
  }
}

namespace {
void CheckCurrentNameValuePair(HttpUtil::NameValuePairsIterator* parser,
                               bool expect_valid,
                               std::string expected_name,
                               std::string expected_value) {
  ASSERT_EQ(expect_valid, parser->valid());
  if (!expect_valid) {
    return;
  }

  // Let's make sure that these never change (i.e., when a quoted value is
  // unquoted, it should be cached on the first calls and not regenerated
  // later).
  std::string::const_iterator first_value_begin = parser->value_begin();
  std::string::const_iterator first_value_end = parser->value_end();

  ASSERT_EQ(expected_name, std::string(parser->name_begin(),
                                       parser->name_end()));
  ASSERT_EQ(expected_name, parser->name());
  ASSERT_EQ(expected_value, std::string(parser->value_begin(),
                                        parser->value_end()));
  ASSERT_EQ(expected_value, parser->value());

  // Make sure they didn't/don't change.
  ASSERT_TRUE(first_value_begin == parser->value_begin());
  ASSERT_TRUE(first_value_end == parser->value_end());
}

void CheckNextNameValuePair(HttpUtil::NameValuePairsIterator* parser,
                            bool expect_next,
                            bool expect_valid,
                            std::string expected_name,
                            std::string expected_value) {
  ASSERT_EQ(expect_next, parser->GetNext());
  ASSERT_EQ(expect_valid, parser->valid());
  if (!expect_next || !expect_valid) {
    return;
  }

  CheckCurrentNameValuePair(parser,
                            expect_valid,
                            expected_name,
                            expected_value);
}

void CheckInvalidNameValuePair(std::string valid_part,
                               std::string invalid_part) {
  std::string whole_string = valid_part + invalid_part;

  HttpUtil::NameValuePairsIterator valid_parser(valid_part.begin(),
                                                valid_part.end(),
                                                ';');
  HttpUtil::NameValuePairsIterator invalid_parser(whole_string.begin(),
                                                  whole_string.end(),
                                                  ';');

  ASSERT_TRUE(valid_parser.valid());
  ASSERT_TRUE(invalid_parser.valid());

  // Both parsers should return all the same values until "valid_parser" is
  // exhausted.
  while (valid_parser.GetNext()) {
    ASSERT_TRUE(invalid_parser.GetNext());
    ASSERT_TRUE(valid_parser.valid());
    ASSERT_TRUE(invalid_parser.valid());
    ASSERT_EQ(valid_parser.name(), invalid_parser.name());
    ASSERT_EQ(valid_parser.value(), invalid_parser.value());
  }

  // valid_parser is exhausted and remains 'valid'
  ASSERT_TRUE(valid_parser.valid());

  // invalid_parser's corresponding call to GetNext also returns false...
  ASSERT_FALSE(invalid_parser.GetNext());
  // ...but the parser is in an invalid state.
  ASSERT_FALSE(invalid_parser.valid());
}

}  // anonymous namespace

TEST(HttpUtilTest, NameValuePairsIteratorCopyAndAssign) {
  std::string data = "alpha='\\'a\\''; beta=\" b \"; cappa='c;'; delta=\"d\"";
  HttpUtil::NameValuePairsIterator parser_a(data.begin(), data.end(), ';');

  EXPECT_TRUE(parser_a.valid());
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser_a, true, true, "alpha", "'a'"));

  HttpUtil::NameValuePairsIterator parser_b(parser_a);
  // a and b now point to same location
  ASSERT_NO_FATAL_FAILURE(
      CheckCurrentNameValuePair(&parser_b, true, "alpha", "'a'"));
  ASSERT_NO_FATAL_FAILURE(
      CheckCurrentNameValuePair(&parser_a, true, "alpha", "'a'"));

  // advance a, no effect on b
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser_a, true, true, "beta", " b "));
  ASSERT_NO_FATAL_FAILURE(
      CheckCurrentNameValuePair(&parser_b, true, "alpha", "'a'"));

  // assign b the current state of a, no effect on a
  parser_b = parser_a;
  ASSERT_NO_FATAL_FAILURE(
      CheckCurrentNameValuePair(&parser_b, true, "beta", " b "));
  ASSERT_NO_FATAL_FAILURE(
      CheckCurrentNameValuePair(&parser_a, true, "beta", " b "));

  // advance b, no effect on a
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser_b, true, true, "cappa", "c;"));
  ASSERT_NO_FATAL_FAILURE(
      CheckCurrentNameValuePair(&parser_a, true, "beta", " b "));
}

TEST(HttpUtilTest, NameValuePairsIteratorEmptyInput) {
  std::string data = "";
  HttpUtil::NameValuePairsIterator parser(data.begin(), data.end(), ';');

  EXPECT_TRUE(parser.valid());
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, false, true, "", ""));
}

TEST(HttpUtilTest, NameValuePairsIterator) {
  std::string data = "alpha=1; beta= 2 ;cappa =' 3; ';"
                     "delta= \" \\\"4\\\" \"; e= \" '5'\"; e=6;"
                     "f='\\'\\h\\e\\l\\l\\o\\ \\w\\o\\r\\l\\d\\'';"
                     "g=''; h='hello'";
  HttpUtil::NameValuePairsIterator parser(data.begin(), data.end(), ';');
  EXPECT_TRUE(parser.valid());

  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "alpha", "1"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "beta", "2"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "cappa", " 3; "));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "delta", " \"4\" "));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "e", " '5'"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "e", "6"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "f", "'hello world'"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "g", ""));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "h", "hello"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, false, true, "", ""));
}

TEST(HttpUtilTest, NameValuePairsIteratorIllegalInputs) {
  ASSERT_NO_FATAL_FAILURE(CheckInvalidNameValuePair("alpha=1", "; beta"));
  ASSERT_NO_FATAL_FAILURE(CheckInvalidNameValuePair("", "beta"));

  ASSERT_NO_FATAL_FAILURE(CheckInvalidNameValuePair("alpha=1", "; 'beta'=2"));
  ASSERT_NO_FATAL_FAILURE(CheckInvalidNameValuePair("", "'beta'=2"));
  ASSERT_NO_FATAL_FAILURE(CheckInvalidNameValuePair("alpha=1", ";beta="));
  ASSERT_NO_FATAL_FAILURE(CheckInvalidNameValuePair("alpha=1",
                                                    ";beta=;cappa=2"));

  // According to the spec this is an error, but it doesn't seem appropriate to
  // change our behaviour to be less permissive at this time.
  // See NameValuePairsIteratorExtraSeparators test
  // ASSERT_NO_FATAL_FAILURE(CheckInvalidNameValuePair("alpha=1", ";; beta=2"));
}

// If we are going to support extra separators against the spec, let's just make
// sure they work rationally.
TEST(HttpUtilTest, NameValuePairsIteratorExtraSeparators) {
  std::string data = " ; ;;alpha=1; ;; ; beta= 2;cappa=3;;; ; ";
  HttpUtil::NameValuePairsIterator parser(data.begin(), data.end(), ';');
  EXPECT_TRUE(parser.valid());

  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "alpha", "1"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "beta", "2"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "cappa", "3"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, false, true, "", ""));
}

// See comments on the implementation of NameValuePairsIterator::GetNext
// regarding this derogation from the spec.
TEST(HttpUtilTest, NameValuePairsIteratorMissingEndQuote) {
  std::string data = "name='value";
  HttpUtil::NameValuePairsIterator parser(data.begin(), data.end(), ';');
  EXPECT_TRUE(parser.valid());

  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, true, true, "name", "value"));
  ASSERT_NO_FATAL_FAILURE(
      CheckNextNameValuePair(&parser, false, true, "", ""));
}

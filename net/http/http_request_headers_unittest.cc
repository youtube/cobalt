// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_request_headers.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(HttpRequestHeaders, SetRequestLine) {
  HttpRequestHeaders headers;
  headers.SetRequestLine(
      HttpRequestHeaders::kGetMethod, "/foo", "1.1");
  EXPECT_EQ("GET /foo HTTP/1.1\r\n\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, SetHeader) {
  HttpRequestHeaders headers;
  headers.SetHeader("Foo", "bar");
  EXPECT_EQ("Foo: bar\r\n\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, SetMultipleHeaders) {
  HttpRequestHeaders headers;
  headers.SetHeader("Cookie-Monster", "Nom nom nom");
  headers.SetHeader("Domo-Kun", "Loves Chrome");
  EXPECT_EQ("Cookie-Monster: Nom nom nom\r\nDomo-Kun: Loves Chrome\r\n\r\n",
            headers.ToString());
}

TEST(HttpRequestHeaders, SetHeaderTwice) {
  HttpRequestHeaders headers;
  headers.SetHeader("Foo", "bar");
  headers.SetHeader("Foo", "bar");
  EXPECT_EQ("Foo: bar\r\n\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, SetHeaderTwiceCaseInsensitive) {
  HttpRequestHeaders headers;
  headers.SetHeader("Foo", "bar");
  headers.SetHeader("FoO", "Bar");
  EXPECT_EQ("Foo: Bar\r\n\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, SetHeaderTwiceSamePrefix) {
  HttpRequestHeaders headers;
  headers.SetHeader("FooBar", "smokes");
  headers.SetHeader("Foo", "crack");
  EXPECT_EQ("FooBar: smokes\r\nFoo: crack\r\n\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, RemoveHeader) {
  HttpRequestHeaders headers;
  headers.SetHeader("Foo", "bar");
  headers.RemoveHeader("Foo");
  EXPECT_EQ("\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, RemoveHeaderMissingHeader) {
  HttpRequestHeaders headers;
  headers.SetHeader("Foo", "bar");
  headers.RemoveHeader("Bar");
  EXPECT_EQ("Foo: bar\r\n\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, RemoveHeaderCaseInsensitive) {
  HttpRequestHeaders headers;
  headers.SetHeader("Foo", "bar");
  headers.SetHeader("All-Your-Base", "Belongs To Chrome");
  headers.RemoveHeader("foo");
  EXPECT_EQ("All-Your-Base: Belongs To Chrome\r\n\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, AddHeaderFromString) {
  HttpRequestHeaders headers;
  headers.AddHeaderFromString("Foo: bar");
  EXPECT_EQ("Foo: bar\r\n\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, AddHeaderFromStringNoLeadingWhitespace) {
  HttpRequestHeaders headers;
  headers.AddHeaderFromString("Foo:bar");
  EXPECT_EQ("Foo: bar\r\n\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, AddHeaderFromStringMoreLeadingWhitespace) {
  HttpRequestHeaders headers;
  headers.AddHeaderFromString("Foo: \t  \t  bar");
  EXPECT_EQ("Foo: bar\r\n\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, AddHeaderFromStringTrailingWhitespace) {
  HttpRequestHeaders headers;
  headers.AddHeaderFromString("Foo: bar  \t  \t   ");
  EXPECT_EQ("Foo: bar\r\n\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, AddHeaderFromStringLeadingTrailingWhitespace) {
  HttpRequestHeaders headers;
  headers.AddHeaderFromString("Foo: \t    bar\t       ");
  EXPECT_EQ("Foo: bar\r\n\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, AddHeaderFromStringWithEmptyValue) {
  HttpRequestHeaders headers;
  headers.AddHeaderFromString("Foo:");
  EXPECT_EQ("Foo:\r\n\r\n", headers.ToString());
}

TEST(HttpRequestHeaders, MergeFrom) {
  HttpRequestHeaders headers;
  headers.SetHeader("A", "A");
  headers.SetHeader("B", "B");

  HttpRequestHeaders headers2;
  headers2.SetHeader("B", "b");
  headers2.SetHeader("C", "c");
  headers.MergeFrom(headers2);
  EXPECT_EQ("A: A\r\nB: b\r\nC: c\r\n\r\n", headers.ToString());
}

}  // namespace

}  // namespace net

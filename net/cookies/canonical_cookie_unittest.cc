// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/canonical_cookie.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(CanonicalCookieTest, GetCookieSourceFromURL) {
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/test")));
  EXPECT_EQ("file:///tmp/test.html",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("file:///tmp/test.html")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com:1234/")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("https://example.com/")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://user:pwd@example.com/")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/test?foo")));
  EXPECT_EQ("http://example.com/",
            CanonicalCookie::GetCookieSourceFromURL(
                GURL("http://example.com/test#foo")));
}

}  // namespace net

// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cookies/canonical_cookie.h"

#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/cookies/cookie_options.h"
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

TEST(CanonicalCookieTest, Constructor) {
  GURL url("http://www.example.com/test");
  base::Time current_time = base::Time::Now();

  CanonicalCookie cookie(url, "A", "2", "www.example.com", "/test", "", "",
                         current_time, base::Time(), current_time, false,
                         false);
  EXPECT_EQ(url.GetOrigin().spec(), cookie.Source());
  EXPECT_EQ("A", cookie.Name());
  EXPECT_EQ("2", cookie.Value());
  EXPECT_EQ("www.example.com", cookie.Domain());
  EXPECT_EQ("/test", cookie.Path());
  EXPECT_FALSE(cookie.IsSecure());

  CanonicalCookie cookie2(url, "A", "2", "", "", "", "", current_time,
                          base::Time(), current_time, false, false);
  EXPECT_EQ(url.GetOrigin().spec(), cookie.Source());
  EXPECT_EQ("A", cookie2.Name());
  EXPECT_EQ("2", cookie2.Value());
  EXPECT_EQ("", cookie2.Domain());
  EXPECT_EQ("", cookie2.Path());
  EXPECT_FALSE(cookie2.IsSecure());

}

TEST(CanonicalCookieTest, Create) {
  GURL url("http://www.example.com/test/foo.html");
  base::Time creation_time = base::Time::Now();
  CookieOptions options;

  scoped_ptr<CanonicalCookie> cookie(
        CanonicalCookie::Create(url, "A=2", creation_time, options));
  EXPECT_EQ(url.GetOrigin().spec(), cookie->Source());
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ("www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());

  GURL url2("http://www.foo.com");
  cookie.reset(CanonicalCookie::Create(url2, "B=1", creation_time, options));
  EXPECT_EQ(url2.GetOrigin().spec(), cookie->Source());
  EXPECT_EQ("B", cookie->Name());
  EXPECT_EQ("1", cookie->Value());
  EXPECT_EQ("www.foo.com", cookie->Domain());
  EXPECT_EQ("/", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());

  cookie.reset(CanonicalCookie::Create(
      url, "A", "2", "www.example.com", "/test", "", "", creation_time,
      base::Time(), false, false));
  EXPECT_EQ(url.GetOrigin().spec(), cookie->Source());
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ(".www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());

  cookie.reset(CanonicalCookie::Create(
      url, "A", "2", ".www.example.com", "/test", "", "", creation_time,
      base::Time(), false, false));
  EXPECT_EQ(url.GetOrigin().spec(), cookie->Source());
  EXPECT_EQ("A", cookie->Name());
  EXPECT_EQ("2", cookie->Value());
  EXPECT_EQ(".www.example.com", cookie->Domain());
  EXPECT_EQ("/test", cookie->Path());
  EXPECT_FALSE(cookie->IsSecure());
}

}  // namespace net

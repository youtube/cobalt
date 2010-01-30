// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/static_cookie_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/gurl.h"

class StaticCookiePolicyTest : public testing::Test {
 public:
  StaticCookiePolicyTest()
      : url_google_("http://www.google.izzle"),
        url_google_secure_("https://www.google.izzle"),
        url_google_mail_("http://mail.google.izzle"),
        url_google_analytics_("http://www.googleanalytics.izzle") { }
 protected:
  GURL url_google_;
  GURL url_google_secure_;
  GURL url_google_mail_;
  GURL url_google_analytics_;
};

TEST_F(StaticCookiePolicyTest, DefaultPolicyTest) {
  net::StaticCookiePolicy cp;
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_secure_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_mail_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_analytics_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, GURL()));

  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_secure_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_mail_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_analytics_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, GURL()));
}

TEST_F(StaticCookiePolicyTest, AllowAllCookiesTest) {
  net::StaticCookiePolicy cp;
  cp.set_type(net::StaticCookiePolicy::ALLOW_ALL_COOKIES);

  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_secure_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_mail_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_analytics_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, GURL()));

  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_secure_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_mail_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_analytics_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, GURL()));
}

TEST_F(StaticCookiePolicyTest, BlockThirdPartyCookiesTest) {
  net::StaticCookiePolicy cp;
  cp.set_type(net::StaticCookiePolicy::BLOCK_THIRD_PARTY_COOKIES);

  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_secure_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_mail_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, url_google_analytics_));
  EXPECT_TRUE(cp.CanGetCookies(url_google_, GURL()));

  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_secure_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, url_google_mail_));
  EXPECT_FALSE(cp.CanSetCookie(url_google_, url_google_analytics_));
  EXPECT_TRUE(cp.CanSetCookie(url_google_, GURL()));
}

TEST_F(StaticCookiePolicyTest, BlockAllCookiesTest) {
  net::StaticCookiePolicy cp;
  cp.set_type(net::StaticCookiePolicy::BLOCK_ALL_COOKIES);

  EXPECT_FALSE(cp.CanGetCookies(url_google_, url_google_));
  EXPECT_FALSE(cp.CanGetCookies(url_google_, url_google_secure_));
  EXPECT_FALSE(cp.CanGetCookies(url_google_, url_google_mail_));
  EXPECT_FALSE(cp.CanGetCookies(url_google_, url_google_analytics_));
  EXPECT_FALSE(cp.CanGetCookies(url_google_, GURL()));

  EXPECT_FALSE(cp.CanSetCookie(url_google_, url_google_));
  EXPECT_FALSE(cp.CanSetCookie(url_google_, url_google_secure_));
  EXPECT_FALSE(cp.CanSetCookie(url_google_, url_google_mail_));
  EXPECT_FALSE(cp.CanSetCookie(url_google_, url_google_analytics_));
  EXPECT_FALSE(cp.CanSetCookie(url_google_, GURL()));
}

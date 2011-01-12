// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_errors.h"
#include "net/base/static_cookie_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/gurl.h"

class StaticCookiePolicyTest : public testing::Test {
 public:
  StaticCookiePolicyTest()
      : url_google_("http://www.google.izzle"),
        url_google_secure_("https://www.google.izzle"),
        url_google_mail_("http://mail.google.izzle"),
        url_google_analytics_("http://www.googleanalytics.izzle") {
  }
  void SetPolicyType(net::StaticCookiePolicy::Type type) {
    policy_.set_type(type);
  }
  int CanGetCookies(const GURL& url, const GURL& first_party) {
    return policy_.CanGetCookies(url, first_party, NULL);
  }
  int CanSetCookie(const GURL& url, const GURL& first_party) {
    return policy_.CanSetCookie(url, first_party, std::string(), NULL);
  }
 protected:
  net::StaticCookiePolicy policy_;
  GURL url_google_;
  GURL url_google_secure_;
  GURL url_google_mail_;
  GURL url_google_analytics_;
};

TEST_F(StaticCookiePolicyTest, DefaultPolicyTest) {
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_secure_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_mail_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_analytics_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, GURL()));

  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_secure_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_mail_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_analytics_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, GURL()));
}

TEST_F(StaticCookiePolicyTest, AllowAllCookiesTest) {
  SetPolicyType(net::StaticCookiePolicy::ALLOW_ALL_COOKIES);

  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_secure_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_mail_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_analytics_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, GURL()));

  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_secure_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_mail_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_analytics_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, GURL()));
}

TEST_F(StaticCookiePolicyTest, BlockSettingThirdPartyCookiesTest) {
  SetPolicyType(net::StaticCookiePolicy::BLOCK_SETTING_THIRD_PARTY_COOKIES);

  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_secure_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_mail_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_analytics_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, GURL()));

  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_secure_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_mail_));
  EXPECT_NE(net::OK, CanSetCookie(url_google_, url_google_analytics_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, GURL()));
}

TEST_F(StaticCookiePolicyTest, BlockAllThirdPartyCookiesTest) {
  SetPolicyType(net::StaticCookiePolicy::BLOCK_ALL_THIRD_PARTY_COOKIES);

  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_secure_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, url_google_mail_));
  EXPECT_NE(net::OK, CanGetCookies(url_google_, url_google_analytics_));
  EXPECT_EQ(net::OK, CanGetCookies(url_google_, GURL()));

  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_secure_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, url_google_mail_));
  EXPECT_NE(net::OK, CanSetCookie(url_google_, url_google_analytics_));
  EXPECT_EQ(net::OK, CanSetCookie(url_google_, GURL()));
}

TEST_F(StaticCookiePolicyTest, BlockAllCookiesTest) {
  SetPolicyType(net::StaticCookiePolicy::BLOCK_ALL_COOKIES);

  EXPECT_NE(net::OK, CanGetCookies(url_google_, url_google_));
  EXPECT_NE(net::OK, CanGetCookies(url_google_, url_google_secure_));
  EXPECT_NE(net::OK, CanGetCookies(url_google_, url_google_mail_));
  EXPECT_NE(net::OK, CanGetCookies(url_google_, url_google_analytics_));
  EXPECT_NE(net::OK, CanGetCookies(url_google_, GURL()));

  EXPECT_NE(net::OK, CanSetCookie(url_google_, url_google_));
  EXPECT_NE(net::OK, CanSetCookie(url_google_, url_google_secure_));
  EXPECT_NE(net::OK, CanSetCookie(url_google_, url_google_mail_));
  EXPECT_NE(net::OK, CanSetCookie(url_google_, url_google_analytics_));
  EXPECT_NE(net::OK, CanSetCookie(url_google_, GURL()));
}

// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "net/http/http_auth_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

struct UrlData {
  GURL url;
  HttpAuth::Target target;
  bool matches;
};

static UrlData urls[] = {
  { GURL(""),
    HttpAuth::AUTH_NONE, false },
  { GURL("http://foo.cn"),
    HttpAuth::AUTH_PROXY, true },
  { GURL("http://foo.cn"),
    HttpAuth::AUTH_SERVER, false },
  { GURL("http://slashdot.org"),
    HttpAuth::AUTH_NONE, false },
  { GURL("http://www.google.com"),
    HttpAuth::AUTH_SERVER, true },
  { GURL("http://www.google.com"),
    HttpAuth::AUTH_PROXY, true },
  { GURL("https://login.facebook.com/login.php?login_attempt=1"),
    HttpAuth::AUTH_NONE, false },
  { GURL("http://codereview.chromium.org/634002/show"),
    HttpAuth::AUTH_SERVER, true },
  { GURL("http://code.google.com/p/chromium/issues/detail?id=34505"),
    HttpAuth::AUTH_SERVER, true },
  { GURL("http://code.google.com/p/chromium/issues/list?can=2&q=label:"
         "spdy&sort=owner&colspec=ID%20Stars%20Pri%20Area%20Type%20Status%20"
         "Summary%20Modified%20Owner%20Mstone%20OS"),
    HttpAuth::AUTH_SERVER, true },
  { GURL("https://www.linkedin.com/secure/login?trk=hb_signin"),
    HttpAuth::AUTH_SERVER, true },
  { GURL("http://www.linkedin.com/mbox?displayMBoxItem=&"
         "itemID=I1717980652_2&trk=COMM_HP_MSGVW_MEBC_MEBC&goback=.hom"),
    HttpAuth::AUTH_SERVER, true },
  { GURL("http://news.slashdot.org/story/10/02/18/190236/"
         "New-Plan-Lets-Top-HS-Students-Graduate-2-Years-Early"),
    HttpAuth::AUTH_PROXY, true },
  { GURL("http://codereview.chromium.org/646068/diff/4001/5003"),
    HttpAuth::AUTH_SERVER, true },
  { GURL("http://codereview.chromium.gag/646068/diff/4001/5003"),
    HttpAuth::AUTH_SERVER, true },
  { GURL("http://codereview.chromium.gog/646068/diff/4001/5003"),
    HttpAuth::AUTH_SERVER, true },
};

}   // namespace

TEST(HttpAuthFilterTest, EmptyFilter) {
  // Create an empty filter
  HttpAuthFilterWhitelist filter;
  for (size_t i = 0; i < arraysize(urls); i++) {
    EXPECT_EQ(urls[i].target == HttpAuth::AUTH_PROXY,
              filter.IsValid(urls[i].url, urls[i].target))
        << " " << i << ": " << urls[i].url;
  }
}

TEST(HttpAuthFilterTest, NonEmptyFilter) {
  // Create an non-empty filter
  HttpAuthFilterWhitelist filter;
  std::string server_filter =
      "*google.com,*linkedin.com,*book.com,*.chromium.org,*.gag,*gog";
  filter.SetFilters(server_filter);
  for (size_t i = 0; i < arraysize(urls); i++) {
    EXPECT_EQ(urls[i].matches, filter.IsValid(urls[i].url, urls[i].target))
        << " " << i << ": " << urls[i].url;
  }
}

}   // namespace net

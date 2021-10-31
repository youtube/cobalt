// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/dom/url_utils.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace dom {

TEST(URLUtilsTest, GettersShouldReturnExpectedFormat) {
  URLUtils url_utils(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));

  EXPECT_EQ("https://user:pass@google.com:99/foo;bar?q=a#ref",
            url_utils.href());
  EXPECT_EQ("https:", url_utils.protocol());
  EXPECT_EQ("google.com:99", url_utils.host());
  EXPECT_EQ("google.com", url_utils.hostname());
  EXPECT_EQ("99", url_utils.port());
  EXPECT_EQ("/foo;bar", url_utils.pathname());
  EXPECT_EQ("#ref", url_utils.hash());
  EXPECT_EQ("?q=a", url_utils.search());
}

TEST(URLUtilsTest, SetHref) {
  URLUtils url_utils(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  url_utils.set_href("http://www.youtube.com");
  EXPECT_TRUE(url_utils.url().is_valid());
  EXPECT_EQ("http://www.youtube.com/", url_utils.href());
}

TEST(URLUtilsTest, SetProtocolShouldWorkAsExpected) {
  URLUtils url_utils(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  url_utils.set_protocol("http");
  EXPECT_TRUE(url_utils.url().is_valid());
  EXPECT_EQ("http://user:pass@google.com:99/foo;bar?q=a#ref", url_utils.href());
}

TEST(URLUtilsTest, SetHostShouldWorkAsExpected) {
  URLUtils url_utils(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  url_utils.set_host("youtube.com");
  EXPECT_TRUE(url_utils.url().is_valid());
  EXPECT_EQ("https://user:pass@youtube.com:99/foo;bar?q=a#ref",
            url_utils.href());

  url_utils.set_host("google.com:100");
  EXPECT_TRUE(url_utils.url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:100/foo;bar?q=a#ref",
            url_utils.href());
}

TEST(URLUtilsTest, SetHostnameShouldWorkAsExpected) {
  URLUtils url_utils(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  url_utils.set_hostname("youtube.com");
  EXPECT_TRUE(url_utils.url().is_valid());
  EXPECT_EQ("https://user:pass@youtube.com:99/foo;bar?q=a#ref",
            url_utils.href());
}

TEST(URLUtilsTest, SetPortShouldWorkAsExpected) {
  URLUtils url_utils(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  url_utils.set_port("100");
  EXPECT_TRUE(url_utils.url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:100/foo;bar?q=a#ref",
            url_utils.href());
}

TEST(URLUtilsTest, SetPathnameShouldWorkAsExpected) {
  URLUtils url_utils(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  url_utils.set_pathname("baz");
  EXPECT_TRUE(url_utils.url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:99/baz?q=a#ref", url_utils.href());
}

TEST(URLUtilsTest, SetHashShouldWorkAsExpected) {
  URLUtils url_utils(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  url_utils.set_hash("hash");
  EXPECT_TRUE(url_utils.url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:99/foo;bar?q=a#hash",
            url_utils.href());

  url_utils.set_hash("#hash2");
  EXPECT_TRUE(url_utils.url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:99/foo;bar?q=a#hash2",
            url_utils.href());
}

TEST(URLUtilsTest, SetSearchShouldWorkAsExpected) {
  URLUtils url_utils(GURL("https://user:pass@google.com:99/foo;bar?q=a#ref"));
  url_utils.set_search("b=c");
  EXPECT_TRUE(url_utils.url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:99/foo;bar?b=c#ref",
            url_utils.href());

  url_utils.set_search("?d=e");
  EXPECT_TRUE(url_utils.url().is_valid());
  EXPECT_EQ("https://user:pass@google.com:99/foo;bar?d=e#ref",
            url_utils.href());
}

}  // namespace dom
}  // namespace cobalt

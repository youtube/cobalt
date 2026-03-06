// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/shell/browser/shell.h"

#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

TEST(ShellTest, IsDeepLinkTopicBasic) {
  // Test basic true case.
  EXPECT_TRUE(Shell::IsDeepLinkTopicForTesting(
      GURL("http://example.com?launch=true&topic=music"), "music"));

  // Test different order.
  EXPECT_TRUE(Shell::IsDeepLinkTopicForTesting(
      GURL("http://example.com?topic=music&launch=true"), "music"));

  // Test multiple topics.
  EXPECT_TRUE(Shell::IsDeepLinkTopicForTesting(
      GURL("http://example.com?topic=foo&topic=music&launch=true"), "music"));

  // Test missing launch.
  EXPECT_FALSE(Shell::IsDeepLinkTopicForTesting(
      GURL("http://example.com?topic=music"), "music"));

  // Test missing topic.
  EXPECT_FALSE(Shell::IsDeepLinkTopicForTesting(
      GURL("http://example.com?launch=true"), "music"));

  // Test incorrect topic.
  EXPECT_FALSE(Shell::IsDeepLinkTopicForTesting(
      GURL("http://example.com?launch=true&topic=video"), "music"));
}

TEST(ShellTest, IsDeepLinkTopicRedirect) {
  // Test single redirect
  GURL inner_url("http://example.com?launch=true&topic=music");
  GURL redirect_url = net::AppendQueryParameter(GURL("http://example.com"),
                                                "redirect", inner_url.spec());
  EXPECT_TRUE(Shell::IsDeepLinkTopicForTesting(redirect_url, "music"));

  // Test escaped redirect url
  EXPECT_TRUE(Shell::IsDeepLinkTopicForTesting(
      GURL("http://"
           "example.com?loader=example&redirect=http%3A%2F%2Fexample.com%"
           "3Flaunch%3Dmenu%26topic%3Dmusic"),
      "music"));
}

TEST(ShellTest, IsDeepLinkTopicMaxDepth) {
  GURL url("http://example.com?launch=true&topic=music");

  // Create a URL with 10 layers of redirects
  for (int i = 0; i < 10; ++i) {
    url = net::AppendQueryParameter(GURL("http://example.com"), "redirect",
                                    url.spec());
  }
  // Depth 10 should be allowed.
  EXPECT_TRUE(Shell::IsDeepLinkTopicForTesting(url, "music"));

  // Add 1 more layer of redirect to exceed max depth (11).
  url = net::AppendQueryParameter(GURL("http://example.com"), "redirect",
                                  url.spec());
  // Should fail because depth exceeds kMaxDeepLinkRedirectDepth (10).
  EXPECT_FALSE(Shell::IsDeepLinkTopicForTesting(url, "music"));
}

}  // namespace content

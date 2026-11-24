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

#include "cobalt/shell/browser/splash_cache.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

TEST(SplashCacheTest, CreateSplashRequest) {
  auto request = CreateSplashRequest("https://www.youtube.com/", "splash.webm");
  EXPECT_EQ(GURL("https://www.youtube.com/splash.webm"), request->url);
  EXPECT_EQ("GET", request->method);
}

// The request is resolved against the origin, where the WebApp caches the
// splash assets, even if the splash domain has a path.
TEST(SplashCacheTest, CreateSplashRequestResolvesAgainstOrigin) {
  auto request =
      CreateSplashRequest("https://www.youtube.com/tv/", "splash.png");
  EXPECT_EQ(GURL("https://www.youtube.com/splash.png"), request->url);
}

TEST(SplashCacheTest, CreateSplashMatchOptions) {
  auto match_options = CreateSplashMatchOptions(u"music");
  EXPECT_EQ(u"music", match_options->cache_name);
  EXPECT_TRUE(match_options->query_options.Equals(CreateSplashQueryOptions()));
}

}  // namespace content

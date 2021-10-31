// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/csp/source.h"

#include "cobalt/csp/content_security_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace csp {

namespace {

class SourceTest : public ::testing::Test {
 public:
  SourceTest() : csp_(new ContentSecurityPolicy(GURL(), ViolationCallback())) {}

 protected:
  std::unique_ptr<ContentSecurityPolicy> csp_;
};

TEST_F(SourceTest, BasicMatching) {
  SourceConfig config;
  config.scheme = "http";
  config.host = "example.com";
  config.path = "/foo/";
  config.port = 8000;

  Source source(csp_.get(), config);

  EXPECT_TRUE(source.Matches(GURL("http://example.com:8000/foo/")));
  EXPECT_TRUE(source.Matches(GURL("http://example.com:8000/foo/bar")));
  EXPECT_TRUE(source.Matches(GURL("HTTP://EXAMPLE.com:8000/foo/BAR")));

  EXPECT_FALSE(source.Matches(GURL("http://example.com:8000/")));
  EXPECT_FALSE(source.Matches(GURL("http://example.com:8000/bar/")));
  EXPECT_FALSE(source.Matches(GURL("https://example.com:8000/bar/")));
  EXPECT_FALSE(source.Matches(GURL("http://example.com:9000/bar/")));
  EXPECT_FALSE(source.Matches(GURL("http://example.com:9000/foo/")));
  EXPECT_FALSE(source.Matches(GURL("http://foo.example.com:8000/")));
}

TEST_F(SourceTest, SpecificPath) {
  SourceConfig config;
  config.scheme = "http";
  config.host = "example.com";
  // Match a specific path.
  config.path = "/foo";

  Source source(csp_.get(), config);

  EXPECT_TRUE(source.Matches(GURL("http://example.com/foo")));
  EXPECT_TRUE(source.Matches(GURL("http://example.com:80/foo")));

  EXPECT_FALSE(source.Matches(GURL("http://example.com/foo.jpg")));
  EXPECT_FALSE(source.Matches(GURL("HTTP://EXAMPLE.com/foo/BAR")));
  EXPECT_FALSE(source.Matches(GURL("http://example.com:8000/")));
}

TEST_F(SourceTest, WildcardMatching) {
  SourceConfig config;
  config.scheme = "http";
  config.host = "example.com";
  config.path = "/";
  config.host_wildcard = SourceConfig::kHasWildcard;
  config.port_wildcard = SourceConfig::kHasWildcard;

  Source source(csp_.get(), config);

  EXPECT_TRUE(source.Matches(GURL("http://foo.example.com:8000/")));
  EXPECT_TRUE(source.Matches(GURL("http://foo.example.com:8000/foo")));
  EXPECT_TRUE(source.Matches(GURL("http://foo.example.com:9000/foo/")));
  EXPECT_TRUE(source.Matches(GURL("http://foo.example.com:8000/")));
  EXPECT_TRUE(source.Matches(GURL("HTTP://FOO.EXAMPLE.com:8000/foo/BAR")));

  EXPECT_FALSE(source.Matches(GURL("http://example.com:8000/")));
  EXPECT_FALSE(source.Matches(GURL("http://example.com:8000/foo")));
  EXPECT_FALSE(source.Matches(GURL("http://example.com:9000/foo/")));
  EXPECT_FALSE(source.Matches(GURL("http://example.foo.com:8000/")));
  EXPECT_FALSE(source.Matches(GURL("https://example.foo.com:8000/")));
  EXPECT_FALSE(source.Matches(GURL("https://example.com:8000/bar/")));
}

TEST_F(SourceTest, RedirectMatching) {
  SourceConfig config;
  config.scheme = "http";
  config.host = "example.com";
  config.path = "/bar/";
  config.port = 8000;

  Source source(csp_.get(), config);

  EXPECT_TRUE(source.Matches(GURL("http://example.com:8000/"),
                             ContentSecurityPolicy::kDidRedirect));
  EXPECT_TRUE(source.Matches(GURL("http://example.com:8000/foo"),
                             ContentSecurityPolicy::kDidRedirect));
  EXPECT_TRUE(source.Matches(GURL("https://example.com:8000/foo"),
                             ContentSecurityPolicy::kDidRedirect));

  EXPECT_FALSE(source.Matches(GURL("http://not-example.com:8000/foo"),
                              ContentSecurityPolicy::kDidRedirect));
  EXPECT_FALSE(source.Matches(GURL("http://example.com:9000/foo/")));
}

TEST_F(SourceTest, InsecureSourceMatchesSecure) {
  SourceConfig config;
  config.scheme = "http";
  config.path = "/";
  config.port_wildcard = SourceConfig::kHasWildcard;
  Source source(csp_.get(), config);

  EXPECT_TRUE(source.Matches(GURL("http://example.com:8000/")));
  EXPECT_TRUE(source.Matches(GURL("https://example.com:8000/")));
  EXPECT_TRUE(source.Matches(GURL("http://not-example.com:8000/")));
  EXPECT_TRUE(source.Matches(GURL("https://not-example.com:8000/")));

  EXPECT_FALSE(source.Matches(GURL("ftp://example.com:8000/")));
}

TEST_F(SourceTest, SecureSourceDoesntMatchInsecure) {
  SourceConfig config;
  config.scheme = "https";
  config.path = "/";
  config.port_wildcard = SourceConfig::kHasWildcard;
  Source source(csp_.get(), config);

  EXPECT_TRUE(source.Matches(GURL("https://example.com:8000/")));
  EXPECT_TRUE(source.Matches(GURL("https://example.com:8000/")));
  EXPECT_TRUE(source.Matches(GURL("https://not-example.com:8000/")));

  EXPECT_FALSE(source.Matches(GURL("http://not-example.com:8000/")));
  EXPECT_FALSE(source.Matches(GURL("ftp://example.com:8000/")));
}

TEST_F(SourceTest, InsecureHostMatchesSecure) {
  SourceConfig config;
  config.scheme = "http";
  config.host = "example.com";
  config.path = "/";
  config.port_wildcard = SourceConfig::kHasWildcard;
  Source source(csp_.get(), config);

  EXPECT_TRUE(source.Matches(GURL("http://example.com:8000/")));
  EXPECT_TRUE(source.Matches(GURL("https://example.com:8000/")));

  EXPECT_FALSE(source.Matches(GURL("http://not-example.com:8000/")));
  EXPECT_FALSE(source.Matches(GURL("https://not-example.com:8000/")));
}

TEST_F(SourceTest, SecureHostDoesntMatchesInsecure) {
  SourceConfig config;
  config.scheme = "https";
  config.host = "example.com";
  config.path = "/";
  config.port_wildcard = SourceConfig::kHasWildcard;
  Source source(csp_.get(), config);

  EXPECT_TRUE(source.Matches(GURL("https://example.com:8000/")));

  EXPECT_FALSE(source.Matches(GURL("http://not-example.com:8000/")));
  EXPECT_FALSE(source.Matches(GURL("http://example.com:8000/")));
  EXPECT_FALSE(source.Matches(GURL("https://not-example.com:8000/")));
}
}  // namespace

}  // namespace csp
}  // namespace cobalt

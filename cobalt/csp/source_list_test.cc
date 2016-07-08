/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/csp/source_list.h"

#include "cobalt/csp/content_security_policy.h"
#include "cobalt/csp/source.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace csp {

namespace {
void ParseSourceList(SourceList* source_list, const std::string& sources) {
  base::StringPiece characters(sources);
  source_list->Parse(characters);
}

class SourceListTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    GURL secure_url("https://example.test/image.png");
    csp_.reset(new ContentSecurityPolicy(secure_url, violation_callback_));
  }

  scoped_ptr<ContentSecurityPolicy> csp_;
  ViolationCallback violation_callback_;
};

TEST_F(SourceListTest, BasicMatchingNone) {
  std::string sources = "'none'";
  SourceList source_list(csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_FALSE(source_list.Matches(GURL("http://example.com/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example.test/")));
}

TEST_F(SourceListTest, BasicMatchingStar) {
  std::string sources = "*";
  SourceList source_list(csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("http://example.com/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example.com/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://example.com/bar")));
  EXPECT_TRUE(source_list.Matches(GURL("http://foo.example.com/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://foo.example.com/bar")));

  EXPECT_FALSE(source_list.Matches(GURL("data:https://example.test/")));
  EXPECT_FALSE(source_list.Matches(GURL("blob:https://example.test/")));
  EXPECT_FALSE(source_list.Matches(GURL("filesystem:https://example.test/")));
}

TEST_F(SourceListTest, BasicMatchingSelf) {
  std::string sources = "'self'";
  SourceList source_list(csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_FALSE(source_list.Matches(GURL("http://example.com/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://not-example.com/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example.test/")));
}

TEST_F(SourceListTest, BlobMatchingSelf) {
  std::string sources = "'self'";
  SourceList source_list(csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("https://example.test/")));
  EXPECT_FALSE(source_list.Matches(GURL("blob:https://example.test/")));

  // TODO: Blink has special code to permit this.
  // EXPECT_TRUE(source_list.Matches(GURL("https://example.test/")));
  // EXPECT_TRUE(source_list.Matches(GURL("blob:https://example.test/")));
}

TEST_F(SourceListTest, BlobMatchingBlob) {
  std::string sources = "blob:";
  SourceList source_list(csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_FALSE(source_list.Matches(GURL("https://example.test/")));
  EXPECT_TRUE(source_list.Matches(GURL("blob:https://example.test/")));
}

TEST_F(SourceListTest, BasicMatching) {
  std::string sources = "http://example1.com:8000/foo/ https://example2.com/";
  SourceList source_list(csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com:8000/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com:8000/foo/bar")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example2.com/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example2.com/foo/")));

  EXPECT_FALSE(source_list.Matches(GURL("https://not-example.com/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://example1.com/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example1.com/foo")));
  EXPECT_FALSE(source_list.Matches(GURL("http://example1.com:9000/foo/")));
}

TEST_F(SourceListTest, WildcardMatching) {
  std::string sources =
      "http://example1.com:*/foo/ https://*.example2.com/bar/ http://*.test/";
  SourceList source_list(csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com:8000/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com:9000/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://foo.example2.com/bar/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://foo.test/")));
  EXPECT_TRUE(source_list.Matches(GURL("http://foo.bar.test/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example1.com/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example1.com:8000/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://example1.com:9000/foo/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://foo.test/")));
  EXPECT_TRUE(source_list.Matches(GURL("https://foo.bar.test/")));

  EXPECT_FALSE(source_list.Matches(GURL("https://example1.com:8000/foo")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example2.com:8000/bar")));
  EXPECT_FALSE(source_list.Matches(GURL("https://foo.example2.com:8000/bar")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example2.foo.com/bar")));
  EXPECT_FALSE(source_list.Matches(GURL("http://foo.test.bar/")));
  EXPECT_FALSE(source_list.Matches(GURL("https://example2.com/bar/")));
  EXPECT_FALSE(source_list.Matches(GURL("http://test/")));
}

TEST_F(SourceListTest, RedirectMatching) {
  std::string sources = "http://example1.com/foo/ http://example2.com/bar/";
  SourceList source_list(csp_.get(), "script-src");
  ParseSourceList(&source_list, sources);

  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com/foo/"),
                                  ContentSecurityPolicy::kDidRedirect));
  EXPECT_TRUE(source_list.Matches(GURL("http://example1.com/bar/"),
                                  ContentSecurityPolicy::kDidRedirect));
  EXPECT_TRUE(source_list.Matches(GURL("http://example2.com/bar/"),
                                  ContentSecurityPolicy::kDidRedirect));
  EXPECT_TRUE(source_list.Matches(GURL("http://example2.com/foo/"),
                                  ContentSecurityPolicy::kDidRedirect));
  EXPECT_TRUE(source_list.Matches(GURL("https://example1.com/foo/"),
                                  ContentSecurityPolicy::kDidRedirect));
  EXPECT_TRUE(source_list.Matches(GURL("https://example1.com/bar/"),
                                  ContentSecurityPolicy::kDidRedirect));

  EXPECT_FALSE(source_list.Matches(GURL("http://example3.com/foo/"),
                                   ContentSecurityPolicy::kDidRedirect));
}

}  // namespace

}  // namespace csp
}  // namespace cobalt

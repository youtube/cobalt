/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/csp/content_security_policy.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace csp {

namespace {

using ::testing::Return;
using ::testing::StrictMock;

class CSPTest : public ::testing::Test {
 public:
  CSPTest() : csp_(new ContentSecurityPolicy()) {}

 protected:
  scoped_ptr<ContentSecurityPolicy> csp_;
};

class MockDelegate : public ContentSecurityPolicy::Delegate {
 public:
  MOCK_CONST_METHOD0(url, GURL(void));
};

TEST_F(CSPTest, SecureSchemeMatchesSelf) {
  StrictMock<MockDelegate> delegate;
  ON_CALL(delegate, url())
      .WillByDefault(Return(GURL("https://www.example.com")));
  csp_->BindDelegate(&delegate);

  EXPECT_CALL(delegate, url()).Times(1);
  csp_->OnReceiveHeader("default-src 'self'", kHeaderTypeEnforce,
                        kHeaderSourceHTTP);

  EXPECT_TRUE(csp_->SchemeMatchesSelf(GURL("https://example.com")));
  EXPECT_FALSE(csp_->SchemeMatchesSelf(GURL("http://example.com")));
}

TEST_F(CSPTest, InsecureSchemeMatchesSelf) {
  StrictMock<MockDelegate> delegate;
  ON_CALL(delegate, url())
      .WillByDefault(Return(GURL("http://www.example.com")));
  csp_->BindDelegate(&delegate);

  EXPECT_CALL(delegate, url()).Times(1);
  csp_->OnReceiveHeader("default-src 'self'", kHeaderTypeEnforce,
                        kHeaderSourceHTTP);

  EXPECT_TRUE(csp_->SchemeMatchesSelf(GURL("https://example.com")));
  EXPECT_TRUE(csp_->SchemeMatchesSelf(GURL("http://example.com")));
}

}  // namespace

}  // namespace csp
}  // namespace cobalt

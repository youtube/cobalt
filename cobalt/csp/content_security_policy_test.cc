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

#include <memory>

#include "cobalt/csp/content_security_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace csp {

namespace {

class CspTest : public ::testing::Test {
 protected:
  ViolationCallback callback_;
  std::unique_ptr<ContentSecurityPolicy> csp_;
};

TEST_F(CspTest, SecureSchemeMatchesSelf) {
  csp_.reset(
      new ContentSecurityPolicy(GURL("https://www.example.com"), callback_));
  EXPECT_TRUE(csp_->SchemeMatchesSelf(GURL("https://example.com")));
  EXPECT_FALSE(csp_->SchemeMatchesSelf(GURL("http://example.com")));
}

TEST_F(CspTest, InsecureSchemeMatchesSelf) {
  csp_.reset(
      new ContentSecurityPolicy(GURL("http://www.example.com"), callback_));
  EXPECT_TRUE(csp_->SchemeMatchesSelf(GURL("https://example.com")));
  EXPECT_TRUE(csp_->SchemeMatchesSelf(GURL("http://example.com")));
}

}  // namespace

}  // namespace csp
}  // namespace cobalt

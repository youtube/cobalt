// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/cobalt_secure_navigation_throttle.h"

#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace cobalt {
namespace browser {

using content::CobaltSecureNavigationThrottle;
using ::testing::NiceMock;
using ::testing::ReturnRef;

const GURL kHttpUrl("http://neverssl.com");
const GURL kHttpsUrl("https://www.youtube.com");

class CobaltSecureNavigationThrottleTest : public ::testing::Test {
 protected:
  CobaltSecureNavigationThrottleTest() {}

  content::NavigationThrottle::ThrottleCheckResult RunWillStartRequest(
      const GURL& url) {
    mock_navigation_handle_.set_url(url);
    throttle_ = std::make_unique<CobaltSecureNavigationThrottle>(
        &mock_navigation_handle_);
    return throttle_->WillStartRequest();
  }

  content::NavigationThrottle::ThrottleCheckResult RunWillRedirectRequest(
      const GURL& url) {
    mock_navigation_handle_.set_url(url);
    throttle_ = std::make_unique<CobaltSecureNavigationThrottle>(
        &mock_navigation_handle_);
    return throttle_->WillRedirectRequest();
  }

  content::NavigationThrottle::ThrottleCheckResult RunWillProcessResponse() {
    mock_navigation_handle_.set_response_headers(response_headers_);
    throttle_ = std::make_unique<CobaltSecureNavigationThrottle>(
        &mock_navigation_handle_);
    return throttle_->WillProcessResponse();
  }

  // Use the pre-existing content::MockNavigationHandle with NiceMock
  NiceMock<content::MockNavigationHandle> mock_navigation_handle_;
  std::unique_ptr<CobaltSecureNavigationThrottle> throttle_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
};

TEST_F(CobaltSecureNavigationThrottleTest, AllowsHttpsRequest) {
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillStartRequest(kHttpsUrl);
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

TEST_F(CobaltSecureNavigationThrottleTest, BlocksHttpRequest) {
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillStartRequest(kHttpUrl);
  if (ShouldEnforceHTTPS(*base::CommandLine::ForCurrentProcess())) {
    EXPECT_EQ(content::NavigationThrottle::CANCEL, result.action());
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, result.net_error_code());
  }
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

TEST_F(CobaltSecureNavigationThrottleTest, AllowsHttpsRedirect) {
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillRedirectRequest(kHttpsUrl);
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

TEST_F(CobaltSecureNavigationThrottleTest, BlocksHttpRedirect) {
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillRedirectRequest(kHttpUrl);
  if (ShouldEnforceHTTPS(*base::CommandLine::ForCurrentProcess())) {
    EXPECT_EQ(content::NavigationThrottle::CANCEL, result.action());
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, result.net_error_code());
  }
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

TEST_F(CobaltSecureNavigationThrottleTest, AllowsCSPResponse) {
  response_headers_ = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n");
  ASSERT_TRUE(response_headers_);

  response_headers_->AddHeader("Content-Security-Policy",
                               "default-src 'self'\n");
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillProcessResponse();
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

TEST_F(CobaltSecureNavigationThrottleTest, BlocksNonCSPResponse) {
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillProcessResponse();
  if (ShouldEnforceCSP(*base::CommandLine::ForCurrentProcess())) {
    EXPECT_EQ(content::NavigationThrottle::CANCEL, result.action());
    EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, result.net_error_code());
  }
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

TEST_F(CobaltSecureNavigationThrottleTest, MissingResponseHeaders) {
  response_headers_ = nullptr;
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillProcessResponse();
#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  EXPECT_EQ(content::NavigationThrottle::CANCEL, result.action());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, result.net_error_code());
#else
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
#endif  // COBALT_IS_RELEASE_BUILD
}

}  // namespace browser
}  // namespace cobalt

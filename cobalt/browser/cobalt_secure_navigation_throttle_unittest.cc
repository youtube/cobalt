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

#include "base/command_line.h"
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
using ::testing::Return;
using ::testing::ReturnRef;

const GURL kHttpUrl("http://neverssl.com");
const GURL kHttpsUrl("https://www.youtube.com");

class MockCobaltSecureNavigationThrottle
    : public content::CobaltSecureNavigationThrottle {
 public:
  explicit MockCobaltSecureNavigationThrottle(
      content::NavigationHandle* navigation_handle)
      : CobaltSecureNavigationThrottle(navigation_handle) {}

  MOCK_METHOD(bool,
              ShouldEnforceHTTPS,
              (const base::CommandLine& command_line),
              (override));
  MOCK_METHOD(bool,
              ShouldEnforceCSP,
              (const base::CommandLine& command_line),
              (override));
};

class CobaltSecureNavigationThrottleTest : public ::testing::Test {
 protected:
  CobaltSecureNavigationThrottleTest() {
    base::CommandLine::Init(0, nullptr);
    throttle_ = std::make_unique<NiceMock<MockCobaltSecureNavigationThrottle>>(
        &mock_navigation_handle_);
  }

  content::NavigationThrottle::ThrottleCheckResult RunWillStartRequest(
      const GURL& url) {
    mock_navigation_handle_.set_url(url);
    return throttle_->WillStartRequest();
  }

  content::NavigationThrottle::ThrottleCheckResult RunWillRedirectRequest(
      const GURL& url) {
    mock_navigation_handle_.set_url(url);
    return throttle_->WillRedirectRequest();
  }

  content::NavigationThrottle::ThrottleCheckResult RunWillProcessResponse() {
    mock_navigation_handle_.set_response_headers(response_headers_);
    return throttle_->WillProcessResponse();
  }

  // Use the pre-existing content::MockNavigationHandle with NiceMock
  NiceMock<content::MockNavigationHandle> mock_navigation_handle_;
  std::unique_ptr<MockCobaltSecureNavigationThrottle> throttle_;
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
};

TEST_F(CobaltSecureNavigationThrottleTest, AllowsHttpsRequest) {
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillStartRequest(kHttpsUrl);
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

TEST_F(CobaltSecureNavigationThrottleTest,
       BlocksHttpRequestWhenEnforcementEnabled) {
  EXPECT_CALL(*throttle_, ShouldEnforceHTTPS).WillOnce(Return(true));
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillStartRequest(kHttpUrl);
  EXPECT_EQ(content::NavigationThrottle::CANCEL, result.action());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, result.net_error_code());
}

TEST_F(CobaltSecureNavigationThrottleTest,
       AllowsHttpRequestWhenEnforcementDisabled) {
  EXPECT_CALL(*throttle_, ShouldEnforceHTTPS).WillOnce(Return(false));
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillStartRequest(kHttpUrl);
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

TEST_F(CobaltSecureNavigationThrottleTest, AllowsHttpsRedirect) {
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillRedirectRequest(kHttpsUrl);
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

TEST_F(CobaltSecureNavigationThrottleTest,
       BlocksHttpRedirectWhenEnforcementEnabled) {
  EXPECT_CALL(*throttle_, ShouldEnforceHTTPS).WillOnce(Return(true));
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillRedirectRequest(kHttpUrl);
  EXPECT_EQ(content::NavigationThrottle::CANCEL, result.action());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, result.net_error_code());
}

TEST_F(CobaltSecureNavigationThrottleTest,
       AllowsHttpRedirectWhenEnforcementDisabled) {
  EXPECT_CALL(*throttle_, ShouldEnforceHTTPS).WillOnce(Return(false));
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillRedirectRequest(kHttpUrl);
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

TEST_F(CobaltSecureNavigationThrottleTest,
       AllowsResponseWithCSPHeaderWhenEnforcementEnabled) {
  response_headers_ = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n");
  ASSERT_TRUE(response_headers_);

  response_headers_->AddHeader("Content-Security-Policy", "default-src 'self'");

  EXPECT_CALL(*throttle_, ShouldEnforceCSP).WillOnce(Return(true));
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillProcessResponse();
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

TEST_F(CobaltSecureNavigationThrottleTest,
       BlocksNonCSPResponseWhenEnforcementEnabled) {
  response_headers_ = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n");
  ASSERT_TRUE(response_headers_);
  EXPECT_CALL(*throttle_, ShouldEnforceCSP).WillOnce(Return(true));
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillProcessResponse();
  EXPECT_EQ(content::NavigationThrottle::CANCEL, result.action());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, result.net_error_code());
}

TEST_F(CobaltSecureNavigationThrottleTest,
       AllowsNonCSPResponseWhenEnforcementDisabled) {
  response_headers_ = net::HttpResponseHeaders::TryToCreate(
      "HTTP/1.1 200 OK\n"
      "connection: keep-alive\n"
      "Cache-control: max-age=10000\n");
  ASSERT_TRUE(response_headers_);
  EXPECT_CALL(*throttle_, ShouldEnforceCSP).WillOnce(Return(false));
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillProcessResponse();
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

TEST_F(CobaltSecureNavigationThrottleTest,
       BlocksAboutBlankNavigationWhenEnforcementEnabled) {
  const GURL kAboutBlankUrl("about:blank");
  EXPECT_CALL(*throttle_, ShouldEnforceHTTPS).WillOnce(Return(true));
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillStartRequest(kAboutBlankUrl);
  EXPECT_EQ(content::NavigationThrottle::CANCEL, result.action());
}

TEST_F(CobaltSecureNavigationThrottleTest,
       AllowsAboutBlankNavigationWhenEnforcementDisabled) {
  const GURL kAboutBlankUrl("about:blank");
  EXPECT_CALL(*throttle_, ShouldEnforceHTTPS).WillOnce(Return(false));
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillStartRequest(kAboutBlankUrl);
  EXPECT_EQ(content::NavigationThrottle::PROCEED, result.action());
}

#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
TEST_F(CobaltSecureNavigationThrottleTest,
       MissingResponseHeadersCancelsInReleaseBuilds) {
  response_headers_ = nullptr;
  EXPECT_CALL(*throttle_, ShouldEnforceCSP).WillOnce(Return(true));
  content::NavigationThrottle::ThrottleCheckResult result =
      RunWillProcessResponse();
  EXPECT_EQ(content::NavigationThrottle::CANCEL, result.action());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, result.net_error_code());
}
#endif  // COBALT_IS_RELEASE_BUILD

}  // namespace browser
}  // namespace cobalt

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

#include "cobalt/browser/cobalt_https_only_navigation_throttle.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_navigation_throttle_inserter.h"
#include "content/public/test/test_renderer_host.h" // For RenderViewHostTestHarness
#include "mojo/core/embedder/embedder.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace browser {

using content::NavigationThrottle;
using content::CobaltHttpsOnlyNavigationThrottle;
const GURL kHttpTestUrl("http://www.neverssl.com");
const GURL kHttpsTestUrl("https://www.youtube.com/tv/");

class CobaltHttpsOnlyNavigationThrottleTest
    : public content::RenderViewHostTestHarness {
 public:
  CobaltHttpsOnlyNavigationThrottleTest() = default;

  // void SetUp() override {
  //   mojo::core::Init();
  //   content::RenderViewHostTestHarness::SetUp();
  //   // Any per-test setup can go here.
  // }

  // void TearDown() override {
  //   // Any per-test teardown.
  //   content::RenderViewHostTestHarness::TearDown();
  // }

  // Static method to create the throttle for the TestNavigationThrottleInserter.
  static std::unique_ptr<NavigationThrottle> CreateThrottleForNavigation(
      CobaltHttpsOnlyNavigationThrottleTest* test_fixture, // Can be null if not needed
      content::NavigationHandle* handle) {
    return std::make_unique<CobaltHttpsOnlyNavigationThrottle>(handle);
  }

  // Simulates a navigation and returns the throttle's final decision.
  // This will test the throttle's behavior at WillStartRequest.
  NavigationThrottle::ThrottleCheckResult SimulateNavigationStart(
      const GURL& url) {
    auto throttle_inserter =
        std::make_unique<content::TestNavigationThrottleInserter>(
            web_contents(),
            base::BindRepeating(
                &CobaltHttpsOnlyNavigationThrottleTest::CreateThrottleForNavigation,
                base::Unretained(this))); // Pass 'this' if CreateThrottleForNavigation needs it

    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(url, main_rfh());

    simulator->Start(); // This triggers WillStartRequest
    return simulator->GetLastThrottleCheckResult();
  }

  // Simulates a redirect to test the throttle's behavior at WillRedirectRequest.
  NavigationThrottle::ThrottleCheckResult SimulateNavigationRedirect(
      const GURL& initial_url,
      const GURL& redirect_url) {
    auto throttle_inserter =
        std::make_unique<content::TestNavigationThrottleInserter>(
            web_contents(),
            base::BindRepeating(
                &CobaltHttpsOnlyNavigationThrottleTest::CreateThrottleForNavigation,
                 base::Unretained(this)));

    std::unique_ptr<content::NavigationSimulator> simulator =
        content::NavigationSimulator::CreateRendererInitiated(initial_url, main_rfh());

    simulator->Start();
    if (simulator->GetLastThrottleCheckResult().action() !=
        NavigationThrottle::PROCEED) {
      return simulator->GetLastThrottleCheckResult();
    }

    simulator->Redirect(redirect_url); // This triggers WillRedirectRequest
    return simulator->GetLastThrottleCheckResult();
  }

 private:
  CobaltHttpsOnlyNavigationThrottleTest(
      const CobaltHttpsOnlyNavigationThrottleTest&) = delete;
  CobaltHttpsOnlyNavigationThrottleTest& operator=(
      const CobaltHttpsOnlyNavigationThrottleTest&) = delete;
};

// Test case for HTTPS requests at WillStartRequest
TEST_F(CobaltHttpsOnlyNavigationThrottleTest, AllowsHttpsRequest) {
  NavigationThrottle::ThrottleCheckResult result =
      SimulateNavigationStart(kHttpsTestUrl);

  EXPECT_EQ(NavigationThrottle::PROCEED, result.action());
}

// Test case for HTTP requests at WillStartRequest
TEST_F(CobaltHttpsOnlyNavigationThrottleTest, BlocksHttpRequest) {
  NavigationThrottle::ThrottleCheckResult result =
      SimulateNavigationStart(kHttpTestUrl);

  EXPECT_EQ(NavigationThrottle::CANCEL, result.action());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, result.net_error_code());
}

// Test case for HTTPS redirects at WillRedirectRequest
TEST_F(CobaltHttpsOnlyNavigationThrottleTest, AllowsHttpsRedirect) {
  NavigationThrottle::ThrottleCheckResult result =
      SimulateNavigationRedirect(kHttpsTestUrl, kHttpsTestUrl);

  EXPECT_EQ(NavigationThrottle::PROCEED, result.action());
}

// Test case for HTTP redirects at WillRedirectRequest
TEST_F(CobaltHttpsOnlyNavigationThrottleTest, BlocksHttpRedirect) {
  NavigationThrottle::ThrottleCheckResult result =
      SimulateNavigationRedirect(kHttpsTestUrl, kHttpTestUrl);

  EXPECT_EQ(NavigationThrottle::CANCEL, result.action());
  EXPECT_EQ(net::ERR_BLOCKED_BY_CLIENT, result.net_error_code());
}

}  // namespace browser
}  // namespace cobalt
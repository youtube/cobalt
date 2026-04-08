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

#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_IOS_TVOS)
#include "cobalt/browser/h5vcc_system/h5vcc_system_impl_base.h"  // nogncheck
#endif

namespace cobalt {

class H5vccSystemBrowserTest : public content::ContentBrowserTest {
 public:
  H5vccSystemBrowserTest() = default;
  ~H5vccSystemBrowserTest() override = default;

#if BUILDFLAG(IS_IOS_TVOS)
 protected:
  void TearDown() override {
    h5vcc_system::H5vccSystemImpl::ResetTestState();
    content::ContentBrowserTest::TearDown();
  }
#endif
};

IN_PROC_BROWSER_TEST_F(H5vccSystemBrowserTest, VerifyApiPresence) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Verify that window.h5vcc exists.
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(),
                              "typeof window.h5vcc !== 'undefined'")
                  .ExtractBool());

  // Verify that window.h5vcc.system exists.
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(),
                              "typeof window.h5vcc.system !== 'undefined'")
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemBrowserTest, VerifySystemProperties) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Verify advertisingId is a string (might be empty, but should be a string).
  EXPECT_TRUE(
      content::EvalJs(shell()->web_contents(),
                      "typeof window.h5vcc.system.advertisingId === 'string'")
          .ExtractBool());

  // Verify limitAdTracking is a boolean.
  EXPECT_TRUE(content::EvalJs(
                  shell()->web_contents(),
                  "typeof window.h5vcc.system.limitAdTracking === 'boolean'")
                  .ExtractBool());

  // Verify userOnExitStrategy is one of the expected numbers.
  // 0: close, 1: minimize, 2: no-exit
  std::string check_strategy = R"(
    (function() {
      const strategy = window.h5vcc.system.userOnExitStrategy;
      return strategy === 0 || strategy === 1 || strategy === 2;
    })()
  )";
  EXPECT_TRUE(
      content::EvalJs(shell()->web_contents(), check_strategy).ExtractBool());
}

#if BUILDFLAG(IS_IOS_TVOS)
IN_PROC_BROWSER_TEST_F(H5vccSystemBrowserTest, GetAdvertisingId) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  static const std::string fallback_id = "00000000-0000-0000-0000-000000000000";
  static const std::string test_id = "12345678-1234-1234-1234-123456789000";

  // Verify that advertisingId returns the fallback value when the tracking
  // authorization is not granted.
  EXPECT_EQ(fallback_id, content::EvalJs(shell()->web_contents(),
                                         "window.h5vcc.system.advertisingId")
                             .ExtractString());

  // Allow tracking authorization and set the test value.
  h5vcc_system::H5vccSystemImpl::SetTrackingAuthorizationForTest(true);
  h5vcc_system::H5vccSystemImpl::SetAdvertisingIdForTest(test_id);

  // Check if advertisingId matches the test value.
  EXPECT_EQ(test_id, content::EvalJs(shell()->web_contents(),
                                     "window.h5vcc.system.advertisingId")
                         .ExtractString());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemBrowserTest, GetLimitAdTracking) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Verify that limitAdTracking returns true when  when the tracking
  // authorization is not granted.
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(),
                              "window.h5vcc.system.limitAdTracking")
                  .ExtractBool());

  // Allow the tracking authorization for testing.
  h5vcc_system::H5vccSystemImpl::SetTrackingAuthorizationForTest(true);

  // Check if limitAdTracking returns false after the tracking authorization is
  // granted.
  EXPECT_FALSE(content::EvalJs(shell()->web_contents(),
                               "window.h5vcc.system.limitAdTracking")
                   .ExtractBool());
}
#endif

}  // namespace cobalt

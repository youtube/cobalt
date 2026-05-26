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

#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {

class H5vccRuntimeBrowserTest : public content::ContentBrowserTest {
 public:
  H5vccRuntimeBrowserTest() = default;
  ~H5vccRuntimeBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    // Simulate a deep link being passed at startup.
    browser::DeepLinkManager::GetInstance()->set_deep_link(
        "https://example.com/test_link");
  }
};

IN_PROC_BROWSER_TEST_F(H5vccRuntimeBrowserTest, GetInitialDeepLink) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // 1. Verify that window.h5vcc.runtime exists.
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(),
                              "typeof window.h5vcc.runtime !== 'undefined'")
                  .ExtractBool());

  // 2. Verify that the initial deep link is correctly returned.
  EXPECT_EQ("https://example.com/test_link",
            content::EvalJs(shell()->web_contents(),
                            "window.h5vcc.runtime.initialDeepLink"));
}

IN_PROC_BROWSER_TEST_F(H5vccRuntimeBrowserTest,
                       InitialDeepLinkClearedAfterAccess) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // 1. Verify that the first page gets the link.
  // It was retrieved and cleared in the browser process during H5vccRuntime
  // construction.
  EXPECT_EQ("https://example.com/test_link",
            content::EvalJs(shell()->web_contents(),
                            "window.h5vcc.runtime.initialDeepLink"));

  // 2. Verify that the manager's link is now empty in the browser process.
  EXPECT_TRUE(browser::DeepLinkManager::GetInstance()->get_deep_link().empty());

  // 3. Navigate to a new page.
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url2));

  // 4. Verify that the new page (and its new H5vccRuntime instance) sees an
  // empty initial deep link.
  EXPECT_EQ("", content::EvalJs(shell()->web_contents(),
                                "window.h5vcc.runtime.initialDeepLink"));
}

}  // namespace cobalt

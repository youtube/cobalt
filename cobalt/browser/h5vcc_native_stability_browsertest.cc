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

#include "cobalt/build/configs/buildflags.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {

class H5vccNativeStabilityBrowserTest : public content::ContentBrowserTest {
 public:
  H5vccNativeStabilityBrowserTest() = default;
  ~H5vccNativeStabilityBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(H5vccNativeStabilityBrowserTest, VerifyApiPresence) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Verify that window.h5vcc and window.h5vcc.nativeStability exist, and that
  // wasLowMemoryKilled has been removed from window.h5vcc.system.
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(),
                              "typeof window.h5vcc !== 'undefined'")
                  .ExtractBool());
  EXPECT_TRUE(
      content::EvalJs(shell()->web_contents(),
                      "typeof window.h5vcc.nativeStability !== 'undefined'")
          .ExtractBool());
  EXPECT_TRUE(
      content::EvalJs(shell()->web_contents(),
                      "typeof window.h5vcc.system.wasLowMemoryKilled === "
                      "'undefined'")
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(H5vccNativeStabilityBrowserTest,
                       VerifyWasLowMemoryKilled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Verify that wasLowMemoryKilled() exists and returns a Promise.
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(),
                              "typeof "
                              "window.h5vcc.nativeStability.wasLowMemoryKilled "
                              "=== 'function'")
                  .ExtractBool());
  EXPECT_TRUE(
      content::EvalJs(shell()->web_contents(),
                      "window.h5vcc.nativeStability.wasLowMemoryKilled() "
                      "instanceof Promise")
          .ExtractBool());

  // Verify that the promise resolves to a boolean and is false initially.
  EXPECT_TRUE(
      content::EvalJs(shell()->web_contents(),
                      "(async () => typeof (await "
                      "window.h5vcc.nativeStability.wasLowMemoryKilled()) === "
                      "'boolean')()")
          .ExtractBool());
  EXPECT_FALSE(
      content::EvalJs(shell()->web_contents(),
                      "(async () => await "
                      "window.h5vcc.nativeStability.wasLowMemoryKilled())()")
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(H5vccNativeStabilityBrowserTest,
                       VerifyWasLowMemoryKilledMultipleCalls) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Verify that concurrent and consecutive calls in the same document resolve
  // consistently and without error.
  std::string test_multiple_calls = R"(
    (async () => {
      const [r1, r2] = await Promise.all([
        window.h5vcc.nativeStability.wasLowMemoryKilled(),
        window.h5vcc.nativeStability.wasLowMemoryKilled(),
      ]);
      const r3 = await window.h5vcc.nativeStability.wasLowMemoryKilled();
      return r1 === false && r2 === false && r3 === false;
    })()
  )";
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), test_multiple_calls)
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(H5vccNativeStabilityBrowserTest,
                       VerifyWasLowMemoryKilledCrossNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url1));

  EXPECT_FALSE(
      content::EvalJs(shell()->web_contents(),
                      "(async () => await "
                      "window.h5vcc.nativeStability.wasLowMemoryKilled())()")
          .ExtractBool());

  // Navigate to a new page (new LocalDOMWindow and new H5vccNativeStability
  // instance).
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url2));

  EXPECT_FALSE(
      content::EvalJs(shell()->web_contents(),
                      "(async () => await "
                      "window.h5vcc.nativeStability.wasLowMemoryKilled())()")
          .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(H5vccNativeStabilityBrowserTest,
                       VerifyWasLowMemoryKilledInFlightNavigationNoCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url1));

  // Trigger wasLowMemoryKilled without awaiting and immediately initiate
  // navigation to a second page to verify in-flight IPC cancellation and
  // document destruction safety in single-process mode.
  EXPECT_TRUE(
      content::ExecJs(shell()->web_contents(),
                      "window.h5vcc.nativeStability.wasLowMemoryKilled();"));

  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url2));

  EXPECT_FALSE(
      content::EvalJs(shell()->web_contents(),
                      "(async () => await "
                      "window.h5vcc.nativeStability.wasLowMemoryKilled())()")
          .ExtractBool());
}

#if !BUILDFLAG(USE_EVERGREEN)
IN_PROC_BROWSER_TEST_F(H5vccNativeStabilityBrowserTest,
                       VerifyReportApisRejectOnNonEvergreen) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  std::string test_get_pending_reports_rejects = R"(
    (async () => {
      try {
        await window.h5vcc.nativeStability.getPendingReports();
        return false;
      } catch (e) {
        return e.name === 'NotSupportedError';
      }
    })()
  )";
  EXPECT_TRUE(
      content::EvalJs(shell()->web_contents(), test_get_pending_reports_rejects)
          .ExtractBool());

  std::string test_ack_reports_rejects = R"(
    (async () => {
      try {
        await window.h5vcc.nativeStability.acknowledgeReports(['uuid-1']);
        return false;
      } catch (e) {
        return e.name === 'NotSupportedError';
      }
    })()
  )";
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), test_ack_reports_rejects)
                  .ExtractBool());
}
#endif  // !BUILDFLAG(USE_EVERGREEN)

}  // namespace cobalt

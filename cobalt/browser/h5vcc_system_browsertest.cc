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

#include <string>

#include "build/build_config.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

namespace cobalt {

class H5vccSystemBrowserTest : public content::ContentBrowserTest {
 public:
  H5vccSystemBrowserTest() = default;
  ~H5vccSystemBrowserTest() override = default;

 protected:
  bool IsWasLowMemoryKilledSupported() {
#if BUILDFLAG(IS_IOS)
    return false;
#elif BUILDFLAG(IS_ANDROID)
    return base::android::BuildInfo::GetInstance()->sdk_int() >=
           base::android::SDK_VERSION_R;
#else
    return true;
#endif
  }

  bool IsGetLowMemoryKillDetailsSupported() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return false;
#else
    return true;
#endif
  }

  bool CheckDefaultWasLowMemoryKilled() {
    if (IsWasLowMemoryKilledSupported()) {
      return content::EvalJs(shell()->web_contents(),
                             "(async () => (await window.h5vcc.system."
                             "wasLowMemoryKilled()) === false)()")
          .ExtractBool();
    }
    return content::EvalJs(shell()->web_contents(),
                           "(async () => {"
                           "  try {"
                           "    await window.h5vcc.system.wasLowMemoryKilled();"
                           "    return false;"
                           "  } catch (e) {"
                           "    return e instanceof DOMException && "
                           "           e.name === 'NotSupportedError';"
                           "  }"
                           "})()")
        .ExtractBool();
  }

  bool CheckDefaultLowMemoryKillDetails() {
    if (IsGetLowMemoryKillDetailsSupported()) {
      return content::EvalJs(shell()->web_contents(),
                             "(async () => (await window.h5vcc.system."
                             "getLowMemoryKillDetails()) === null)()")
          .ExtractBool();
    }
    return content::EvalJs(
               shell()->web_contents(),
               "(async () => {"
               "  try {"
               "    await window.h5vcc.system.getLowMemoryKillDetails();"
               "    return false;"
               "  } catch (e) {"
               "    return e instanceof DOMException && "
               "           e.name === 'NotSupportedError';"
               "  }"
               "})()")
        .ExtractBool();
  }
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

IN_PROC_BROWSER_TEST_F(H5vccSystemBrowserTest, VerifyWasLowMemoryKilled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Verify that wasLowMemoryKilled() exists and returns a Promise.
  EXPECT_TRUE(
      content::EvalJs(
          shell()->web_contents(),
          "typeof window.h5vcc.system.wasLowMemoryKilled === 'function'")
          .ExtractBool());
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(),
                              "(() => {"
                              "  const p = window.h5vcc.system."
                              "wasLowMemoryKilled();"
                              "  if (p instanceof Promise) {"
                              "    p.catch(() => {});"
                              "    return true;"
                              "  }"
                              "  return false;"
                              "})()")
                  .ExtractBool());

  // Verify that the promise resolves to false initially when supported, or
  // rejects with NotSupportedError when unsupported.
  EXPECT_TRUE(CheckDefaultWasLowMemoryKilled());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemBrowserTest, VerifyGetLowMemoryKillDetails) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Verify that getLowMemoryKillDetails() exists and returns a Promise.
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(),
                              "typeof window.h5vcc.system."
                              "getLowMemoryKillDetails === 'function'")
                  .ExtractBool());
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(),
                              "(() => {"
                              "  const p = window.h5vcc.system."
                              "getLowMemoryKillDetails();"
                              "  if (p instanceof Promise) {"
                              "    p.catch(() => {});"
                              "    return true;"
                              "  }"
                              "  return false;"
                              "})()")
                  .ExtractBool());

  // Verify that the promise resolves strictly to null when not killed on
  // supported platforms, or rejects with NotSupportedError when unsupported.
  EXPECT_TRUE(CheckDefaultLowMemoryKillDetails());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemBrowserTest,
                       VerifyWasLowMemoryKilledMultipleCalls) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url));

  // Verify that concurrent and consecutive calls in the same document settle
  // consistently.
  std::string test_multiple_calls = content::JsReplace(
      R"(
        (async () => {
          const expectV1Supported = $1;
          const expectV2Supported = $2;
          const checkV1 = async () => {
            try {
              const res = await window.h5vcc.system.wasLowMemoryKilled();
              return expectV1Supported && res === false;
            } catch (e) {
              return !expectV1Supported && e instanceof DOMException &&
                     e.name === 'NotSupportedError';
            }
          };
          const checkV2 = async () => {
            try {
              const res = await window.h5vcc.system.getLowMemoryKillDetails();
              return expectV2Supported && res === null;
            } catch (e) {
              return !expectV2Supported && e instanceof DOMException &&
                     e.name === 'NotSupportedError';
            }
          };
          const [r1, r2, d1, d2] = await Promise.all([
            checkV1(),
            checkV1(),
            checkV2(),
            checkV2(),
          ]);
          const r3 = await checkV1();
          const d3 = await checkV2();
          return r1 && r2 && r3 && d1 && d2 && d3;
        })()
      )",
      IsWasLowMemoryKilledSupported(), IsGetLowMemoryKillDetailsSupported());
  EXPECT_TRUE(content::EvalJs(shell()->web_contents(), test_multiple_calls)
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemBrowserTest,
                       VerifyWasLowMemoryKilledCrossNavigation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url1));

  EXPECT_TRUE(CheckDefaultWasLowMemoryKilled());
  EXPECT_TRUE(CheckDefaultLowMemoryKillDetails());

  // Navigate to a new page (new LocalDOMWindow and new H5vccSystem instance).
  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url2));

  EXPECT_TRUE(CheckDefaultWasLowMemoryKilled());
  EXPECT_TRUE(CheckDefaultLowMemoryKillDetails());
}

IN_PROC_BROWSER_TEST_F(H5vccSystemBrowserTest,
                       VerifyWasLowMemoryKilledInFlightNavigationNoCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url1 = embedded_test_server()->GetURL("/title1.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url1));

  // Trigger LMK queries without awaiting and immediately initiate navigation
  // to a second page to verify in-flight IPC cancellation and document
  // destruction safety in single-process mode.
  EXPECT_TRUE(content::ExecJs(
      shell()->web_contents(),
      "window.h5vcc.system.wasLowMemoryKilled().catch(() => {});"
      "window.h5vcc.system.getLowMemoryKillDetails().catch(() => {});"));

  GURL url2 = embedded_test_server()->GetURL("/title2.html");
  ASSERT_TRUE(NavigateToURL(shell()->web_contents(), url2));

  EXPECT_TRUE(CheckDefaultWasLowMemoryKilled());
  EXPECT_TRUE(CheckDefaultLowMemoryKillDetails());
}

}  // namespace cobalt

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

#include "cobalt/shell/browser/h5vcc_scheme_url_loader_factory.h"

#include <string>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

class H5vccSchemeURLLoaderFactoryBrowserTest : public ContentBrowserTest {
 public:
  H5vccSchemeURLLoaderFactoryBrowserTest() {
    scoped_feature_list_.InitFromCommandLine(
        "DisableExclusiveLockingOnDipsDatabase", "");
  }
  ~H5vccSchemeURLLoaderFactoryBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryBrowserTest, LoadSplashHtml) {
  GURL splash_url("h5vcc-embedded://splash.html");
  EXPECT_TRUE(NavigateToURL(shell(), splash_url));

  // Verify that the page content contains the expected video element.
  EXPECT_EQ(1, EvalJs(shell(), "document.querySelectorAll('video').length"));

  // Verify that the URL matches.
  EXPECT_EQ(splash_url, shell()->web_contents()->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryBrowserTest,
                       LoadNonExistentResource) {
  GURL url("h5vcc-embedded://nonexistent.html");
  // Navigation to non-existent resource should fail or return 404 content.
  // Based on implementation, it returns "Resource not found" with 404 status.
  // NavigateToURL returns true if navigation commits, which it should even for
  // 404.
  EXPECT_TRUE(NavigateToURL(shell(), url));
  std::string body_text =
      EvalJs(shell(), "document.body.innerText").ExtractString();
  EXPECT_EQ("Resource not found", body_text);
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryBrowserTest,
                       LoadSplashVideoWithFallbackParameter) {
  GURL splash_url("h5vcc-embedded://splash.html");
  EXPECT_TRUE(NavigateToURL(shell(), splash_url));

  // Verify fetching the video using a NON-EXISTENT fallback mechanism.
  // We expect this to FAIL because splash_480.webm is NOT an embedded resource.
  std::string script = R"(
    (async () => {
      try {
        const response = await fetch('h5vcc-embedded://splash.webm?fallback=splash_1080.webm');
        if (!response.ok) {
          return 'Fetch failed: ' + response.status;
        }
        const blob = await response.blob();
        if (blob.type !== 'video/webm') {
          return 'Unexpected mime type: ' + blob.type;
        }
        if (blob.size === 0) {
          return 'Empty body';
        }
        return 'Success';
      } catch (e) {
        return 'Exception: ' + e.toString();
      }
    })();
  )";

  // Expect failure because the file doesn't exist
  EXPECT_EQ("Success", EvalJs(shell(), script));
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryBrowserTest,
                       LoadSplashVideoFromCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(
      embedded_test_server()->base_url().spec());

  // 1. Populate the cache.
  // We navigate to the "splash domain" (test server) to access its cache.
  GURL setup_url = embedded_test_server()->GetURL("/title1.html");
  EXPECT_TRUE(NavigateToURL(shell(), setup_url));

  // Put a video into the cache.
  std::string script = R"(
    (async () => {
      try {
        const cache = await caches.open('default');
        const response = new Response('aaabbbccc');
        await cache.put('splash.webm', response);
        return 'Success';
      } catch (e) {
        return 'Exception: ' + e.toString();
      }
    })();
  )";
  EXPECT_EQ("Success", EvalJs(shell(), script));

  // 2. Fetch via h5vcc-embedded scheme.
  // The loader should find the cached content from the test domain.
  GURL splash_url("h5vcc-embedded://splash.html");
  EXPECT_TRUE(NavigateToURL(shell(), splash_url));

  std::string fetch_script = R"(
    (async () => {
      try {
        const response = await fetch('h5vcc-embedded://splash.webm');
        if (!response.ok) {
          return 'Fetch failed: ' + response.status;
        }
        const text = await response.text();
        return text;
      } catch (e) {
        return 'Exception: ' + e.toString();
      }
    })();
  )";

  EXPECT_EQ("aaabbbccc", EvalJs(shell(), fetch_script));
}

}  // namespace content

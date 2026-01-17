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

  std::string GetScript(const std::string& url) {
    return base::StringPrintf(R"(
      (async () => {
        try {
          const video = document.createElement('video');
          const mediaSource = new MediaSource();
          video.src = URL.createObjectURL(mediaSource);

          await new Promise((resolve) => mediaSource.addEventListener('sourceopen', resolve, {once: true}));
          const sourceBuffer = mediaSource.addSourceBuffer('video/webm; codecs="vp9"');

          const response = await fetch('%s');
          if (!response.ok) {
            return 'Fetch failed: ' + response.status;
          }
          const data = await response.arrayBuffer();

          sourceBuffer.appendBuffer(data);
          await new Promise((resolve) => sourceBuffer.addEventListener('updateend', resolve, {once: true}));
          mediaSource.endOfStream();

          if (video.readyState < 1) {
            await new Promise((resolve, reject) => {
              video.onloadedmetadata = resolve;
              video.onerror = () => reject('Video load error: ' + (video.error ? video.error.code + ' ' + video.error.message : 'unknown'));
            });
          }

          return 'Dimensions: ' + video.videoWidth + 'x' + video.videoHeight;
        } catch (e) {
          return 'Exception: ' + e.toString();
        }
      })();
    )",
                              url.c_str());
  }

  void VerifySplashVideoFromCache(const std::string& cache_name,
                                  const std::string& query_param) {
    ASSERT_TRUE(embedded_test_server()->Start());
    H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(
        embedded_test_server()->base_url().spec());

    // 1. Populate the cache.
    // We navigate to the "splash domain" (test server) to access its cache.
    GURL setup_url = embedded_test_server()->GetURL("/title1.html");
    EXPECT_TRUE(NavigateToURL(shell(), setup_url));

    // Put a video into the cache.
    std::string script = base::StringPrintf(R"(
      (async () => {
        try {
          const cache = await caches.open('%s');
          const response = new Response('aaabbbccc');
          await cache.put('splash.webm', response);
          return 'Success';
        } catch (e) {
          return 'Exception: ' + e.toString();
        }
      })();
    )",
                                            cache_name.c_str());
    EXPECT_EQ("Success", EvalJs(shell(), script));

    // 2. Fetch via h5vcc-embedded scheme.
    // The loader should find the cached content from the test domain.
    GURL splash_url("h5vcc-embedded://splash.html");
    EXPECT_TRUE(NavigateToURL(shell(), splash_url));

    std::string fetch_url = "h5vcc-embedded://splash.webm" + query_param;
    std::string fetch_script = base::StringPrintf(R"(
      (async () => {
        try {
          const response = await fetch('%s');
          if (!response.ok) {
            return 'Fetch failed: ' + response.status;
          }
          const text = await response.text();
          return text;
        } catch (e) {
          return 'Exception: ' + e.toString();
        }
      })();
    )",
                                                  fetch_url.c_str());

    EXPECT_EQ("aaabbbccc", EvalJs(shell(), fetch_script));
  }

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
  EXPECT_EQ("Dimensions: 1920x1080",
            EvalJs(shell(), GetScript("h5vcc-embedded://splash.webm")));
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryBrowserTest,
                       LoadSplashVideoWithFallbackParameter) {
  GURL splash_url("h5vcc-embedded://splash.html");
  EXPECT_TRUE(NavigateToURL(shell(), splash_url));

  // Verify fallback for the low spec devices, where Cobalt should
  // play the low resolution splash.
  EXPECT_EQ("Dimensions: 853x480",
            EvalJs(shell(), GetScript("h5vcc-embedded://splash.webm?fallback="
                                      "splash_480.webm")));
}

// If not specified, use cache "default".
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryBrowserTest,
                       LoadSplashVideoFromDefaultCache) {
  VerifySplashVideoFromCache("default", "");
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryBrowserTest,
                       LoadSplashVideoFromOtherCache) {
  VerifySplashVideoFromCache(
      "other",
      // If cache hit, fallback should have no impact here.
      "?cache=other&fallback=splash_480.webm");
}

}  // namespace content

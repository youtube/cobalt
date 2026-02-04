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
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/shell/common/url_constants.h"
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
  H5vccSchemeURLLoaderFactoryBrowserTest() {}
  ~H5vccSchemeURLLoaderFactoryBrowserTest() override = default;

  void SetUp() override {
    ContentBrowserTest::SetUp();
    H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(std::nullopt);
    H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(nullptr);
  }

  void TearDownOnMainThread() override {
    // Navigate away to ensure clean teardown before the test environment is
    // destroyed.
    if (shell()) {
      EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
    }
    ContentBrowserTest::TearDownOnMainThread();
  }

  void TearDown() override {
    H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(std::nullopt);
    H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(nullptr);
    ContentBrowserTest::TearDown();
  }

  std::string CheckImageDimension() {
    // Mock MediaSource.isTypeSupported for static image fallback
    return R"(
      MediaSource.isTypeSupported = function(mime) {
        return false;
      };
      playSplashAnimation();
      (async () => {
        try {
          const placeholder = document.getElementById('placeholder');
          if (!placeholder) {
            return 'No placeholder element found';
          }

          // check if image is displayed
          const style = window.getComputedStyle(placeholder);
          if (style.backgroundImage === 'none') {
            return 'No image is displayed';
          }
          if (parseFloat(style.opacity) < 1) {
            return 'Image does not have 100% opacity';
          }

          // check image dimension
          const url = style.backgroundImage.replace(/^url\(["']?/, '').replace(/["']?\)$/, '');
          return await new Promise((resolve, reject) => {
            const image = new Image();
            image.onload = () => resolve('Dimensions: ' + image.naturalWidth + 'x' + image.naturalHeight);
            image.onerror = () => reject('Image load error');
            image.src = url;
          });
        } catch (e) {
          return 'Exception: ' + e.toString();
        }
      })();
    )";
  }

  std::string CheckVideoDimension(bool is_type_supported) {
    // Mock MediaSource.isTypeSupported for high/low spec device.
    return base::StringPrintf(R"(
      MediaSource.isTypeSupported = function(mime) {
        // Bypass fallback for static image
        if (!mime.includes('width=3840') || !mime.includes('height=2160')) {
          return true;
        }
        return %s;
      };
      playSplashAnimation();
      (async () => {
        try {
          const video = document.querySelector('video');
          if (!video) {
            return 'No video element found';
          }

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
                              is_type_supported ? "true" : "false");
  }

 private:
  std::unique_ptr<GeneratedResourceMap> test_resource_map;
  base::test::ScopedFeatureList scoped_feature_list_;
};

const char kSafeSplashHtml[] = "<html><body>Safe</body></html>";
const char kBuiltinSplash[] = "BUILTIN_SPLASH";

class H5vccSchemeURLLoaderFactoryCacheBrowserTest
    : public H5vccSchemeURLLoaderFactoryBrowserTest {
 public:
  // Verifies that the splash video is correctly loaded from the cache.
  // |cache_name|: The name of the cache storage to use.
  // |query_param|: URL query parameters to append to the fetch request.
  // |content|: The content to store in the cache.
  // |expected_content|: The expected content to be retrieved.
  void VerifySplashVideoFromCacheWithContent(
      const std::string& cache_name,
      const std::string& query_param,
      const std::string& content,
      const std::string& expected_content) {
    ASSERT_TRUE(embedded_test_server()->Start());
    H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(
        embedded_test_server()->base_url().spec());

    // Use a plain html to simplify the test. This verifies the cache,
    // hence bypass the MediaSource in the splash.html.
    auto test_resource_map = std::make_unique<GeneratedResourceMap>();
    LoaderEmbeddedResources::GenerateMap(*test_resource_map);
    (*test_resource_map)["splash.html"] = safe_contents_;
    (*test_resource_map)["splash.webm"] = builtin_contents_;
    H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(
        test_resource_map.get());

    // 1. Populate the cache.
    // We navigate to the "splash domain" (test server) to access its cache.
    GURL setup_url = embedded_test_server()->GetURL("/title1.html");
    EXPECT_TRUE(NavigateToURL(shell(), setup_url));

    // Put a video into the cache.
    std::string script =
        base::StringPrintf(R"(
      (async () => {
        try {
          const cache = await caches.open('%s');
          const blob = new Blob(['%s'], {type: 'video/webm'});
          const response = new Response(blob);
          await cache.put('splash.webm', response);
          return 'Success';
        } catch (e) {
          return 'Exception: ' + e.toString();
        }
      })();
    )",
                           cache_name.c_str(), content.c_str());
    EXPECT_EQ("Success", EvalJs(shell(), script));

    // 2. Fetch via h5vcc-embedded scheme.
    // The loader should find the cached content from the test domain.
    GURL splash_url(std::string(kH5vccEmbeddedScheme) + "://splash.html");
    EXPECT_TRUE(NavigateToURL(shell(), splash_url));

    std::string fetch_url =
        std::string(kH5vccEmbeddedScheme) + "://splash.webm" + query_param;
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

    std::string result = EvalJs(shell(), fetch_script).ExtractString();
    EXPECT_EQ(expected_content, result);
  }

  void VerifySplashVideoFromCache(const std::string& cache_name,
                                  const std::string& query_param) {
    VerifySplashVideoFromCacheWithContent(cache_name, query_param, "aaabbbccc",
                                          "aaabbbccc");
  }

 protected:
  FileContents safe_contents_ = {
      reinterpret_cast<const unsigned char*>(kSafeSplashHtml),
      sizeof(kSafeSplashHtml) - 1};
  FileContents builtin_contents_ = {
      reinterpret_cast<const unsigned char*>(kBuiltinSplash),
      sizeof(kBuiltinSplash) - 1};
};

// TODO(478334656): Support Media source for Android's
// browser tests. Once they are plumbed, remove the prefix.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_LoadSplashHtml DISABLED_LoadSplashHtml
#else
#define MAYBE_LoadSplashHtml LoadSplashHtml
#endif
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryBrowserTest,
                       MAYBE_LoadSplashHtml) {
  GURL splash_url(std::string(kH5vccEmbeddedScheme) + "://splash.html");
  EXPECT_TRUE(NavigateToURL(shell(), splash_url));

  // Verify that the page content contains the expected video element.
  EXPECT_EQ(1, EvalJs(shell(), "document.querySelectorAll('video').length"));

  // Verify that the URL matches.
  EXPECT_EQ(splash_url, shell()->web_contents()->GetLastCommittedURL());
  EXPECT_EQ("Dimensions: 1920x1080",
            EvalJs(shell(), CheckVideoDimension(true)));
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_LoadSplashVideoWithFallbackParameter \
  DISABLED_LoadSplashVideoWithFallbackParameter
#else
#define MAYBE_LoadSplashVideoWithFallbackParameter \
  LoadSplashVideoWithFallbackParameter
#endif
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryBrowserTest,
                       MAYBE_LoadSplashVideoWithFallbackParameter) {
  GURL splash_url(std::string(kH5vccEmbeddedScheme) + "://splash.html");
  EXPECT_TRUE(NavigateToURL(shell(), splash_url));

  // Verify fallback for the low spec devices, where Cobalt should
  // play the low resolution splash.
  EXPECT_EQ("Dimensions: 853x480", EvalJs(shell(), CheckVideoDimension(false)));
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_LoadStaticImageFallback DISABLED_LoadStaticImageFallback
#else
#define MAYBE_LoadStaticImageFallback LoadStaticImageFallback
#endif
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryBrowserTest,
                       MAYBE_LoadStaticImageFallback) {
  GURL splash_url(std::string(kH5vccEmbeddedScheme) + "://splash.html");
  EXPECT_TRUE(NavigateToURL(shell(), splash_url));

  // verify fall back for static image when device does not support VP9
  EXPECT_EQ("Dimensions: 1920x1080", EvalJs(shell(), CheckImageDimension()));
}

// If not specified, use cache "default".
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryCacheBrowserTest,
                       LoadSplashVideoFromDefaultCache) {
  VerifySplashVideoFromCache("default", "");
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryCacheBrowserTest,
                       LoadSplashVideoFromOtherCache) {
  VerifySplashVideoFromCache(
      "other",
      // If cache hit, fallback should have no impact here.
      "?cache=other&fallback=splash_480.webm");
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryCacheBrowserTest,
                       SplashContentTooLarge) {
  // Default kMaxSplashContentSize is too large for test fixtures.
  // Set a smaller size for testing.
  H5vccSchemeURLLoaderFactory::SetSplashContentSizeForTesting(150);
  std::string large_content(151, 'x');
  VerifySplashVideoFromCacheWithContent("default", "", large_content,
                                        "BUILTIN_SPLASH");
}

}  // namespace content

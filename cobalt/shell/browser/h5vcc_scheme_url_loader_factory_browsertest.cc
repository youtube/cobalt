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

#include <memory>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "cobalt/browser/features.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/shell/common/url_constants.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "cobalt/testing/browser_tests/content_browser_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"
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
    H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(nullptr);
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
      MediaSource.isTypeSupported = () => false;
      (async () => {
        return await new Promise((resolve) => {
          const checkImage = () => {
            try {
              const placeholder = document.getElementById('placeholder');
              if (!placeholder) {
                return resolve('No placeholder element found');
              }

              // check if image is displayed
              const style = window.getComputedStyle(placeholder);
              const bgImage = style.backgroundImage;
              if (bgImage === 'none') {
                return resolve('No image is displayed');
              }
              if (parseFloat(style.opacity) < 1) {
                return resolve('Image does not have 100% opacity');
              }

              // check image dimension
              const url = bgImage.replace(/^url\(["']?/, '').replace(/["']?\)$/, '');
              const image = new Image();
              image.onload = () => resolve('Dimensions: ' + image.naturalWidth + 'x' + image.naturalHeight);
              image.onerror = () => resolve('Image load error');
              image.src = url;
            } catch (e) {
              return resolve('Exception: ' + e.toString());
            }
          }

          // use checkImage as callback to avoid setting timeout
          playSplashAnimation(null, checkImage);
        });
      })();
    )";
  }

  std::string CheckVideoDimension(bool is_4k_supported) {
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
                              is_4k_supported ? "true" : "false");
  }

 protected:
  std::unique_ptr<GeneratedResourceMap> test_resource_map_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

const char kSafeSplashHtml[] = "<html><body>Safe</body></html>";
const char kBuiltinSplash[] = "BUILTIN_SPLASH";
const char kBuiltinSplash480[] = "BUILTIN_SPLASH_480";

class H5vccSchemeURLLoaderFactoryCacheBrowserTest
    : public H5vccSchemeURLLoaderFactoryBrowserTest {
 public:
  H5vccSchemeURLLoaderFactoryCacheBrowserTest()
      : H5vccSchemeURLLoaderFactoryCacheBrowserTest(
            /*enable_cached_splash_file=*/true) {}

  void SetUp() override {
    // Cached splash assets are only read with kEnableCachedSplashFile, which
    // is disabled by default. Browser tests can't override features once the
    // browser starts, e.g., with a ScopedFeatureList in
    // CreatedBrowserMainParts(), which fails a DCHECK on Android, so this
    // overrides it in the factory before the browser starts. Otherwise, the
    // feature's default applies.
    H5vccSchemeURLLoaderFactory::SetCachedSplashFileEnabledForTesting(
        enable_cached_splash_file_ ? std::optional<bool>(true) : std::nullopt);
    H5vccSchemeURLLoaderFactoryBrowserTest::SetUp();
  }

  void TearDown() override {
    H5vccSchemeURLLoaderFactoryBrowserTest::TearDown();
    H5vccSchemeURLLoaderFactory::SetCachedSplashFileEnabledForTesting(
        std::nullopt);
  }

  // Verifies that the splash video is correctly loaded from the cache (or
  // falls back).
  // |cache_name|: The name of the cache storage to use.
  // |query_param|: URL query parameters to append to the fetch request.
  // |content|: The optional content to store in the cache. If nullopt,
  //  no content is put.
  // |expected_content|: The expected content to be retrieved.
  // |expected_uma_state|: The expected UMA state metric
  //  of SplashScreenFetchedState.
  // |create_empty_cache|: If true, creates the cache even if no content
  //  is put.
  void VerifySplashVideoFromCacheWithContent(
      const std::string& cache_name,
      const std::string& query_param,
      const std::optional<std::string>& content,
      const std::string& expected_content,
      SplashScreenFetchedState expected_uma_state,
      bool create_empty_cache = false) {
    base::HistogramTester histogram_tester;

    ASSERT_TRUE(embedded_test_server()->Start());
    H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(
        embedded_test_server()->base_url().spec());

    // Use a plain html to simplify the test. This verifies the cache,
    // hence bypass the MediaSource in the splash.html.
    test_resource_map_ = std::make_unique<GeneratedResourceMap>();
    LoaderEmbeddedResources::GenerateMap(*test_resource_map_);
    (*test_resource_map_)["splash.html"] = safe_contents_;
    (*test_resource_map_)["splash.webm"] = builtin_contents_;
    (*test_resource_map_)["splash_480.webm"] = builtin_480_contents_;
    H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(
        test_resource_map_.get());

    // 1. Populate the cache.
    // We navigate to the "splash domain" (test server) to access its cache.
    GURL setup_url = embedded_test_server()->GetURL("/title1.html");
    EXPECT_TRUE(NavigateToURL(shell(), setup_url));

    if (content.has_value()) {
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
                             cache_name.c_str(), content.value().c_str());
      EXPECT_EQ("Success", EvalJs(shell(), script));
    } else if (create_empty_cache) {
      // Create the cache but don't add the video.
      std::string script = base::StringPrintf(R"(
        (async () => {
          try {
            await caches.open('%s');
            return 'Success';
          } catch (e) {
            return 'Exception: ' + e.toString();
          }
        })();
      )",
                                              cache_name.c_str());
      EXPECT_EQ("Success", EvalJs(shell(), script));
    }

    // 2. Fetch via h5vcc-embedded scheme.
    // The loader should find the cached content from the test domain.
    GURL splash_url(std::string(kH5vccEmbeddedScheme) + "://splash.html");
    EXPECT_TRUE(NavigateToURL(shell(), splash_url));

    std::string fetch_url =
        std::string(kH5vccEmbeddedScheme) + "://splash.webm" + query_param;
    std::string fetch_script = base::StringPrintf(R"(
      (async () => {
        try {
          const t0 = performance.now();
          const response = await fetch('%s');
          if (!response.ok) {
            return 'Fetch failed: ' + response.status;
          }
          const text = await response.text();
          const t1 = performance.now();
          return text + '|' + (t1 - t0);
        } catch (e) {
          return 'Exception: ' + e.toString();
        }
      })();
    )",
                                                  fetch_url.c_str());

    std::string result = EvalJs(shell(), fetch_script).ExtractString();

    size_t separator_pos = result.rfind('|');
    // CHECK this separator_pos.
    CHECK(separator_pos != std::string::npos)
        << "Failed to get cache read time from result: " << result;
    std::string fetched_text = result.substr(0, separator_pos);
    double time_ms;
    if (base::StringToDouble(result.substr(separator_pos + 1), &time_ms)) {
      EXPECT_EQ(expected_content, fetched_text);
      testing::Test::RecordProperty("CacheReadTime_ms",
                                    base::NumberToString(time_ms));
      testing::Test::RecordProperty("CacheReadTime_Trace", cache_name);
      // Keep printing to stdout for legacy perf parsers as well
      perf_test::PrintResult("CacheReadTime", "", cache_name, time_ms, "ms",
                             true);
    } else {
      ADD_FAILURE() << "Failed to parse time from result: " << result;
    }

    histogram_tester.ExpectUniqueSample("Cobalt.SplashScreen.FetchedFromCache",
                                        expected_uma_state, 1);

    H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(nullptr);
  }

  void VerifySplashVideoFromCache(const std::string& cache_name,
                                  const std::string& query_param) {
    VerifySplashVideoFromCacheWithContent(
        /*cache_name=*/cache_name, /*query_param=*/query_param,
        /*content=*/"aaabbbccc", /*expected_content=*/"aaabbbccc",
        /*expected_uma_state=*/SplashScreenFetchedState::kOkCache);
  }

  // Serves the splash domain from the test server, and shows one of its
  // pages, which can populate its cache.
  void ShowSplashDomain() {
    ASSERT_TRUE(embedded_test_server()->Start());
    H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(
        embedded_test_server()->base_url().spec());
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  }

  // Puts `content` as `name` in the cache `cache_name` of the splash domain,
  // whose page ShowSplashDomain() shows.
  void PutInSplashCache(const std::string& cache_name,
                        const std::string& name,
                        std::string_view content) {
    std::string script = base::StringPrintf(
        R"(
      (async () => {
        const bytes = Uint8Array.from(atob('%s'), c => c.charCodeAt(0));
        const cache = await caches.open('%s');
        await cache.put('%s', new Response(new Blob([bytes])));
        return 'Success';
      })();
    )",
        base::Base64Encode(content).c_str(), cache_name.c_str(), name.c_str());
    EXPECT_EQ("Success", EvalJs(shell(), script));
  }

  // Shows a page of the h5vcc-embedded scheme that fetches nothing itself.
  void ShowSafeSplashPage() {
    test_resource_map_ = std::make_unique<GeneratedResourceMap>();
    LoaderEmbeddedResources::GenerateMap(*test_resource_map_);
    (*test_resource_map_)["splash.html"] = safe_contents_;
    H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(
        test_resource_map_.get());
    EXPECT_TRUE(NavigateToURL(
        shell(), GURL(std::string(kH5vccEmbeddedScheme) + "://splash.html")));
  }

  // Returns the assets of splash_assets.json for the cache `cache_name`, the
  // splash screen assets that the WebAssembly policy of splash.html chooses
  // from, or the status with which fetching it failed.
  std::string DescribeSplashAssetList(const std::string& cache_name) {
    ShowSafeSplashPage();
    return EvalJs(shell(), base::StringPrintf(R"(
      (async () => {
        try {
          const response = await fetch(
              'h5vcc-embedded://splash_assets.json?cache=%s');
          if (!response.ok) {
            return 'Fetch failed: ' + response.status;
          }
          const assets = await response.json();
          return response.headers.get('Content-Type') + ': ' +
              assets.map(asset => asset.source === 'cache' ?
                  `cache:${asset.cache}/${asset.name}:${asset.kind}:` +
                      asset.bytes :
                  `builtin:${asset.name}:${asset.kind}:${asset.topic}`)
              .join(',');
        } catch (e) {
          return 'Exception: ' + e.toString();
        }
      })();
    )",
                                              cache_name.c_str()))
        .ExtractString();
  }

 protected:
  explicit H5vccSchemeURLLoaderFactoryCacheBrowserTest(
      bool enable_cached_splash_file)
      : enable_cached_splash_file_(enable_cached_splash_file) {}

  FileContents safe_contents_ = {
      reinterpret_cast<const unsigned char*>(kSafeSplashHtml),
      sizeof(kSafeSplashHtml) - 1};
  FileContents builtin_contents_ = {
      reinterpret_cast<const unsigned char*>(kBuiltinSplash),
      sizeof(kBuiltinSplash) - 1};
  FileContents builtin_480_contents_ = {
      reinterpret_cast<const unsigned char*>(kBuiltinSplash480),
      sizeof(kBuiltinSplash480) - 1};

 private:
  const bool enable_cached_splash_file_;
};

// As in Finch's crash safe mode, or with the kill switch.
class H5vccSchemeURLLoaderFactoryCacheDisabledBrowserTest
    : public H5vccSchemeURLLoaderFactoryCacheBrowserTest {
 public:
  H5vccSchemeURLLoaderFactoryCacheDisabledBrowserTest()
      : H5vccSchemeURLLoaderFactoryCacheBrowserTest(
            /*enable_cached_splash_file=*/false) {}
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
#define MAYBE_LoadMusicStaticImageFallback DISABLED_LoadMusicStaticImageFallback
#else
#define MAYBE_LoadStaticImageFallback LoadStaticImageFallback
#define MAYBE_LoadMusicStaticImageFallback LoadMusicStaticImageFallback
#endif
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryBrowserTest,
                       MAYBE_LoadStaticImageFallback) {
  GURL splash_url(std::string(kH5vccEmbeddedScheme) + "://splash.html");
  EXPECT_TRUE(NavigateToURL(shell(), splash_url));

  // verify fall back for static image when device does not support VP9
  EXPECT_EQ("Dimensions: 484x114", EvalJs(shell(), CheckImageDimension()));
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryBrowserTest,
                       MAYBE_LoadMusicStaticImageFallback) {
  GURL splash_url(std::string(kH5vccEmbeddedScheme) +
                  "://splash.html?cache=music");
  EXPECT_TRUE(NavigateToURL(shell(), splash_url));

  // verify fall back for music static image when device does not support VP9
  EXPECT_EQ("Dimensions: 724x138", EvalJs(shell(), CheckImageDimension()));
}

// splash.html's timeout counts from when its video starts, as Shell's does.
// Unlike the splash.html tests above, it runs on Android too: with the image
// forced, the page doesn't use MediaSource, which Android's browser tests don't
// support yet.
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryBrowserTest,
                       SplashTimeoutCountsFromVideoStart) {
  // The image keeps a real video from starting, while the page still handles
  // the events of its video element, which the test sends itself.
  GURL splash_url(std::string(kH5vccEmbeddedScheme) +
                  "://splash.html?force_image=true&timeout=1000");
  EXPECT_TRUE(NavigateToURL(shell(), splash_url));

  // Returns how long after its video started the page closed itself.
  constexpr char kScript[] = R"(
    (async () => {
      let closedMs = 0;
      window.close = () => { closedMs = closedMs || performance.now(); };
      // The video starts 500 ms after the page has loaded, and ends at once.
      const loadEndMs =
          performance.getEntriesByType('navigation')[0].loadEventEnd;
      await new Promise((resolve) => setTimeout(
          resolve, Math.max(0, loadEndMs + 500 - performance.now())));
      const videoEl = document.getElementById('video');
      const startMs = performance.now();
      videoEl.dispatchEvent(new Event('playing'));
      videoEl.dispatchEvent(new Event('ended'));
      while (!closedMs) {
        await new Promise((resolve) => setTimeout(resolve, 10));
      }
      return closedMs - startMs;
    })();
  )";
  // Counted from the page's load instead, the page would close 500 ms sooner.
  EXPECT_GE(EvalJs(shell(), kScript).ExtractDouble(), 990);
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryCacheBrowserTest,
                       LoadSplashVideoNoCache) {
  VerifySplashVideoFromCacheWithContent(
      /*cache_name=*/"default", /*query_param=*/"", /*content=*/std::nullopt,
      /*expected_content=*/"BUILTIN_SPLASH",
      /*expected_uma_state=*/SplashScreenFetchedState::kOkBuiltIn);
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryCacheBrowserTest,
                       CacheExistsButResourceMissing) {
  VerifySplashVideoFromCacheWithContent(
      /*cache_name=*/"default", /*query_param=*/"", /*content=*/std::nullopt,
      /*expected_content=*/"BUILTIN_SPLASH",
      /*expected_uma_state=*/SplashScreenFetchedState::kOkBuiltIn,
      /*create_empty_cache=*/true);
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryCacheBrowserTest,
                       CacheExistsWithEmptyResource) {
  VerifySplashVideoFromCacheWithContent(
      /*cache_name=*/"default", /*query_param=*/"", /*content=*/"",
      /*expected_content=*/"BUILTIN_SPLASH",
      /*expected_uma_state=*/
      SplashScreenFetchedState::kErrorOnCacheEmptyContent);
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
  VerifySplashVideoFromCacheWithContent(
      /*cache_name=*/"default", /*query_param=*/"", /*content=*/large_content,
      /*expected_content=*/"BUILTIN_SPLASH",
      /*expected_uma_state=*/
      SplashScreenFetchedState::kErrorOnCacheFileOversize);
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryCacheDisabledBrowserTest,
                       IgnoresCachedSplashVideo) {
  VerifySplashVideoFromCacheWithContent(
      /*cache_name=*/"default", /*query_param=*/"", /*content=*/"aaabbbccc",
      /*expected_content=*/"BUILTIN_SPLASH",
      /*expected_uma_state=*/
      SplashScreenFetchedState::kSkippedCachedAssets);
}

// The "fallback" query param still chooses the built-in resource.
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryCacheDisabledBrowserTest,
                       UsesFallbackParameter) {
  VerifySplashVideoFromCacheWithContent(
      /*cache_name=*/"other",
      /*query_param=*/"?cache=other&fallback=splash_480.webm",
      /*content=*/"aaabbbccc", /*expected_content=*/"BUILTIN_SPLASH_480",
      /*expected_uma_state=*/
      SplashScreenFetchedState::kSkippedCachedAssets);
}

// With SplashWasmPolicy, splash.html chooses its asset with a WebAssembly
// policy, from splash_assets.json.
class H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest
    : public H5vccSchemeURLLoaderFactoryCacheBrowserTest {
 public:
  H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest()
      : H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest(
            /*enable_cached_splash_file=*/true) {}

 protected:
  explicit H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest(
      bool enable_cached_splash_file)
      : H5vccSchemeURLLoaderFactoryCacheBrowserTest(enable_cached_splash_file) {
    // In the constructor, so that the browser starts with it.
    wasm_policy_feature_list_.InitAndEnableFeature(
        cobalt::features::kSplashWasmPolicy);
  }

 private:
  base::test::ScopedFeatureList wasm_policy_feature_list_;
};

// Without EnableCachedSplashFile, the policy chooses among the built-in assets.
class H5vccSchemeURLLoaderFactoryWasmPolicyCacheDisabledBrowserTest
    : public H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest {
 public:
  H5vccSchemeURLLoaderFactoryWasmPolicyCacheDisabledBrowserTest()
      : H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest(
            /*enable_cached_splash_file=*/false) {}
};

namespace {

const char kSplashWasmPolicyOutcome[] =
    "Cobalt.SplashScreen.WasmPolicy.Outcome";
const char kSplashWasmPolicyDecisionTime[] =
    "Cobalt.SplashScreen.WasmPolicy.DecisionTime";

// The built-in assets, as DescribeSplashAssetList() describes them.
const char kBuiltInSplashAssets[] =
    "builtin:splash.webm:video:default,"
    "builtin:splash_480.webm:video:default,"
    "builtin:music_splash.webm:video:music,"
    "builtin:music_splash_480.webm:video:music,"
    "builtin:splash.png:image:default,"
    "builtin:music_splash.png:image:music";

// A PNG image of 1x1 pixel.
const char kOnePixelPngBase64[] =
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJ"
    "AAAADUlEQVR42mNk+M9QDwADhgGAWjR9awAAAABJRU5ErkJggg==";

// Compiles an empty WebAssembly module: "Compiled", or the error's name.
const char kCompileWasmScript[] = R"(
  WebAssembly.compile(new Uint8Array([0x00, 0x61, 0x73, 0x6d, 1, 0, 0, 0]))
      .then(() => 'Compiled', error => error.name);
)";

// Returns the size of the image that splash.html shows, once it does.
const char kSplashImageSizeScript[] = R"(
  (async () => {
    const placeholder = document.getElementById('placeholder');
    while (!placeholder.style.backgroundImage) {
      await new Promise((resolve) => setTimeout(resolve, 10));
    }
    const image = new Image();
    image.src = placeholder.style.backgroundImage
        .replace(/^url\(["']?/, '').replace(/["']?\)$/, '');
    await image.decode();
    return image.naturalWidth + 'x' + image.naturalHeight;
  })();
)";

// The splash screen with its image, which doesn't close itself during the
// test, with `params`.
GURL GetSplashImageURL(const std::string& params) {
  return GURL(std::string(kH5vccEmbeddedScheme) +
              "://splash.html?force_image=true&timeout=60000" + params);
}

// Returns the URL of the video that splash.html requests with its policy, for
// `params`, the params of its own choice, and `plays4k`, without the decision
// time, which varies. Runs in the splash screen that GetSplashImageURL()
// shows, as Android's browser tests can't play the video.
std::string GetPolicyVideoURL(Shell* shell,
                              const std::string& params,
                              bool plays4k) {
  const std::string script = base::StringPrintf(
      R"(
    splashUrlFromWasmPolicy('splash.webm', new URLSearchParams('%s'), %s)
        .then(url => url.replace(/&policy_ms=\d+$/, ''));
  )",
      params.c_str(), plays4k ? "true" : "false");
  return EvalJs(shell, script).ExtractString();
}

}  // namespace

// The assets of the cache of the splash screen's topic, which the first
// request for the list chooses here, as Shell isn't running. The list of
// another cache is unavailable, so that splash.html reads that cache itself.
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       ListsBuiltInAndCachedSplashAssets) {
  ShowSplashDomain();
  PutInSplashCache("default", "splash.webm", "aaabbbccc");
  PutInSplashCache("music", "splash.png", "png");
  // Not where splash.html reads its assets.
  PutInSplashCache("default", "music_splash.png", "png");
  PutInSplashCache("default", "tv/splash.webm", "webm");

  EXPECT_EQ(std::string("application/json: ") + kBuiltInSplashAssets +
                ",cache:default/splash.webm:video:9",
            DescribeSplashAssetList("default"));
  EXPECT_EQ("Fetch failed: 503", DescribeSplashAssetList("music"));
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       ListsSplashAssetsOfMusicCache) {
  ShowSplashDomain();
  PutInSplashCache("default", "splash.webm", "aaabbbccc");
  PutInSplashCache("music", "splash.png", "png");

  EXPECT_EQ(std::string("application/json: ") + kBuiltInSplashAssets +
                ",cache:music/splash.png:image:3",
            DescribeSplashAssetList("music"));
  EXPECT_EQ("Fetch failed: 503", DescribeSplashAssetList("default"));
}

IN_PROC_BROWSER_TEST_F(
    H5vccSchemeURLLoaderFactoryWasmPolicyCacheDisabledBrowserTest,
    ListsOnlyBuiltInSplashAssets) {
  ShowSplashDomain();
  PutInSplashCache("default", "splash.webm", "aaabbbccc");

  EXPECT_EQ(std::string("application/json: ") + kBuiltInSplashAssets,
            DescribeSplashAssetList("default"));
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryCacheBrowserTest,
                       DoesNotServeSplashAssetListWithoutWasmPolicy) {
  EXPECT_EQ("Fetch failed: 404", DescribeSplashAssetList("default"));
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       ContentSecurityPolicyAllowsWasm) {
  ShowSafeSplashPage();
  EXPECT_EQ("Compiled", EvalJs(shell(), kCompileWasmScript));
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryCacheBrowserTest,
                       ContentSecurityPolicyForbidsWasmWithoutWasmPolicy) {
  ShowSafeSplashPage();
  EXPECT_EQ("CompileError", EvalJs(shell(), kCompileWasmScript));
}

// The built-in asset that the policy chose is served without reading the
// cache, which has a splash video here.
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       ServesBuiltInPolicyChoiceWithoutReadingCache) {
  VerifySplashVideoFromCacheWithContent(
      /*cache_name=*/"default", /*query_param=*/"?source=builtin",
      /*content=*/"aaabbbccc", /*expected_content=*/"BUILTIN_SPLASH",
      /*expected_uma_state=*/SplashScreenFetchedState::kOkBuiltIn);
}

IN_PROC_BROWSER_TEST_F(
    H5vccSchemeURLLoaderFactoryWasmPolicyCacheDisabledBrowserTest,
    ServesBuiltInPolicyChoiceWithCachedAssetsDisabled) {
  VerifySplashVideoFromCacheWithContent(
      /*cache_name=*/"default", /*query_param=*/"?source=builtin",
      /*content=*/"aaabbbccc", /*expected_content=*/"BUILTIN_SPLASH",
      /*expected_uma_state=*/SplashScreenFetchedState::kSkippedCachedAssets);
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryCacheBrowserTest,
                       IgnoresSourceParameterWithoutWasmPolicy) {
  VerifySplashVideoFromCache("default", "?source=builtin");
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       RecordsBuiltInPolicyChoice) {
  base::HistogramTester histogram_tester;
  VerifySplashVideoFromCacheWithContent(
      /*cache_name=*/"default",
      /*query_param=*/"?source=builtin&policy=builtin&policy_ms=250",
      /*content=*/"aaabbbccc", /*expected_content=*/"BUILTIN_SPLASH",
      /*expected_uma_state=*/SplashScreenFetchedState::kOkBuiltIn);

  histogram_tester.ExpectUniqueSample(
      kSplashWasmPolicyOutcome, SplashWasmPolicyOutcome::kChoseBuiltIn, 1);
  histogram_tester.ExpectUniqueTimeSample(kSplashWasmPolicyDecisionTime,
                                          base::Milliseconds(250), 1);
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       RecordsCachedPolicyChoice) {
  base::HistogramTester histogram_tester;
  VerifySplashVideoFromCache(
      "default",
      "?cache=default&fallback=splash_480.webm&policy=cached&policy_ms=300");

  histogram_tester.ExpectUniqueSample(kSplashWasmPolicyOutcome,
                                      SplashWasmPolicyOutcome::kChoseCached, 1);
  histogram_tester.ExpectUniqueTimeSample(kSplashWasmPolicyDecisionTime,
                                          base::Milliseconds(300), 1);
}

// splash.html makes its own choice, and reports why.
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       RecordsUnavailableSplashAssetList) {
  base::HistogramTester histogram_tester;
  VerifySplashVideoFromCache("default", "?policy=list_unavailable");

  histogram_tester.ExpectUniqueSample(
      kSplashWasmPolicyOutcome, SplashWasmPolicyOutcome::kListUnavailable, 1);
  histogram_tester.ExpectTotalCount(kSplashWasmPolicyDecisionTime, 0);
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       RecordsPolicyFailure) {
  base::HistogramTester histogram_tester;
  VerifySplashVideoFromCache("default", "?policy=failed&policy_ms=300");

  histogram_tester.ExpectUniqueSample(kSplashWasmPolicyOutcome,
                                      SplashWasmPolicyOutcome::kFailed, 1);
  histogram_tester.ExpectTotalCount(kSplashWasmPolicyDecisionTime, 0);
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       RecordsNoPolicyMetricsForUntaggedRequest) {
  base::HistogramTester histogram_tester;
  VerifySplashVideoFromCache("default", "");

  histogram_tester.ExpectTotalCount(kSplashWasmPolicyOutcome, 0);
  histogram_tester.ExpectTotalCount(kSplashWasmPolicyDecisionTime, 0);
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryCacheBrowserTest,
                       RecordsNoPolicyMetricsWithoutWasmPolicy) {
  base::HistogramTester histogram_tester;
  VerifySplashVideoFromCache("default", "?policy=cached&policy_ms=300");

  histogram_tester.ExpectTotalCount(kSplashWasmPolicyOutcome, 0);
  histogram_tester.ExpectTotalCount(kSplashWasmPolicyDecisionTime, 0);
}

// The splash screen shows the image that its policy chose, the built-in one,
// as the WebApp cached none. Unlike the splash.html tests above, these run on
// Android too, as the image doesn't use MediaSource.
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       SplashScreenShowsBuiltInImageThatPolicyChose) {
  base::HistogramTester histogram_tester;
  ShowSplashDomain();
  EXPECT_TRUE(NavigateToURL(shell(), GetSplashImageURL("&wasm_policy=true")));

  EXPECT_EQ("484x114", EvalJs(shell(), kSplashImageSizeScript));
  histogram_tester.ExpectUniqueSample(
      kSplashWasmPolicyOutcome, SplashWasmPolicyOutcome::kChoseBuiltIn, 1);
  histogram_tester.ExpectTotalCount(kSplashWasmPolicyDecisionTime, 1);
  histogram_tester.ExpectUniqueSample("Cobalt.SplashScreen.FetchedFromCache",
                                      SplashScreenFetchedState::kOkBuiltIn, 1);
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       SplashScreenShowsBuiltInMusicImageThatPolicyChose) {
  base::HistogramTester histogram_tester;
  ShowSplashDomain();
  EXPECT_TRUE(NavigateToURL(
      shell(), GetSplashImageURL("&cache=music&wasm_policy=true")));

  EXPECT_EQ("724x138", EvalJs(shell(), kSplashImageSizeScript));
  histogram_tester.ExpectUniqueSample(
      kSplashWasmPolicyOutcome, SplashWasmPolicyOutcome::kChoseBuiltIn, 1);
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       SplashScreenShowsCachedImageThatPolicyChose) {
  base::HistogramTester histogram_tester;
  ShowSplashDomain();
  std::string png;
  ASSERT_TRUE(base::Base64Decode(kOnePixelPngBase64, &png));
  PutInSplashCache("default", "splash.png", png);
  EXPECT_TRUE(NavigateToURL(shell(), GetSplashImageURL("&wasm_policy=true")));

  EXPECT_EQ("1x1", EvalJs(shell(), kSplashImageSizeScript));
  histogram_tester.ExpectUniqueSample(kSplashWasmPolicyOutcome,
                                      SplashWasmPolicyOutcome::kChoseCached, 1);
  histogram_tester.ExpectUniqueSample("Cobalt.SplashScreen.FetchedFromCache",
                                      SplashScreenFetchedState::kOkCache, 1);
}

// The policy chooses the video that splash.html chooses itself: the built-in
// one of the splash screen's topic, the 1080p one only where the platform
// plays 4K, unless the WebApp cached one.
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       PolicyChoosesBuiltInVideo) {
  ShowSplashDomain();
  EXPECT_TRUE(NavigateToURL(shell(), GetSplashImageURL("&wasm_policy=true")));
  EXPECT_EQ("484x114", EvalJs(shell(), kSplashImageSizeScript));

  EXPECT_EQ("h5vcc-embedded://splash.webm?source=builtin&policy=builtin",
            GetPolicyVideoURL(shell(), "cache=default&fallback=splash.webm",
                              /*plays4k=*/true));
  EXPECT_EQ("h5vcc-embedded://splash_480.webm?source=builtin&policy=builtin",
            GetPolicyVideoURL(shell(), "cache=default&fallback=splash_480.webm",
                              /*plays4k=*/false));
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       PolicyChoosesBuiltInMusicVideo) {
  ShowSplashDomain();
  EXPECT_TRUE(NavigateToURL(
      shell(), GetSplashImageURL("&cache=music&wasm_policy=true")));
  EXPECT_EQ("724x138", EvalJs(shell(), kSplashImageSizeScript));

  EXPECT_EQ("h5vcc-embedded://music_splash.webm?source=builtin&policy=builtin",
            GetPolicyVideoURL(shell(), "cache=music&fallback=music_splash.webm",
                              /*plays4k=*/true));
  EXPECT_EQ(
      "h5vcc-embedded://music_splash_480.webm?source=builtin&policy=builtin",
      GetPolicyVideoURL(shell(), "cache=music&fallback=music_splash_480.webm",
                        /*plays4k=*/false));
}

IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       PolicyChoosesCachedVideo) {
  ShowSplashDomain();
  // Before the splash screen loads, as it lists the cached assets then.
  PutInSplashCache("music", "splash.webm", "webm");
  EXPECT_TRUE(NavigateToURL(
      shell(), GetSplashImageURL("&cache=music&wasm_policy=true")));
  EXPECT_EQ("724x138", EvalJs(shell(), kSplashImageSizeScript));

  // Read from the topic's cache, as splash.html reads it, even where the
  // platform doesn't play the built-in 1080p video.
  EXPECT_EQ(
      "h5vcc-embedded://"
      "splash.webm?cache=music&fallback=music_splash_480.webm&policy=cached",
      GetPolicyVideoURL(shell(), "cache=music&fallback=music_splash_480.webm",
                        /*plays4k=*/false));
}

// splash.html makes its own choice if the policy chooses another asset, here
// as the page asks for the 1080p video, which the platform doesn't play.
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       SplashScreenChoosesItselfIfPolicyChoosesOtherVideo) {
  ShowSplashDomain();
  EXPECT_TRUE(NavigateToURL(shell(), GetSplashImageURL("&wasm_policy=true")));
  EXPECT_EQ("484x114", EvalJs(shell(), kSplashImageSizeScript));

  EXPECT_EQ(
      "h5vcc-embedded://"
      "splash.webm?cache=default&fallback=splash.webm&policy=failed",
      GetPolicyVideoURL(shell(), "cache=default&fallback=splash.webm",
                        /*plays4k=*/false));
}

// The splash screen shows its own choice if its policy fails, here as the
// policy isn't a valid WebAssembly module.
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       SplashScreenShowsOwnChoiceIfPolicyFails) {
  base::HistogramTester histogram_tester;
  ShowSplashDomain();
  test_resource_map_ = std::make_unique<GeneratedResourceMap>();
  LoaderEmbeddedResources::GenerateMap(*test_resource_map_);
  (*test_resource_map_)["splash_policy.wasm"] = safe_contents_;
  H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(
      test_resource_map_.get());
  EXPECT_TRUE(NavigateToURL(shell(), GetSplashImageURL("&wasm_policy=true")));

  EXPECT_EQ("484x114", EvalJs(shell(), kSplashImageSizeScript));
  histogram_tester.ExpectUniqueSample(kSplashWasmPolicyOutcome,
                                      SplashWasmPolicyOutcome::kFailed, 1);
  histogram_tester.ExpectTotalCount(kSplashWasmPolicyDecisionTime, 0);
  histogram_tester.ExpectUniqueSample("Cobalt.SplashScreen.FetchedFromCache",
                                      SplashScreenFetchedState::kOkBuiltIn, 1);
}

// Without the "wasm_policy" query param, the splash screen chooses its asset
// itself, as without the feature.
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryWasmPolicyBrowserTest,
                       SplashScreenChoosesItselfWithoutWasmPolicyParameter) {
  base::HistogramTester histogram_tester;
  ShowSplashDomain();
  EXPECT_TRUE(NavigateToURL(shell(), GetSplashImageURL("")));

  EXPECT_EQ("484x114", EvalJs(shell(), kSplashImageSizeScript));
  histogram_tester.ExpectTotalCount(kSplashWasmPolicyOutcome, 0);
  histogram_tester.ExpectUniqueSample("Cobalt.SplashScreen.FetchedFromCache",
                                      SplashScreenFetchedState::kOkBuiltIn, 1);
}

// Without SplashWasmPolicy, the policy can't run, as neither splash_assets.json
// nor WebAssembly is available, and the splash screen shows its own choice.
IN_PROC_BROWSER_TEST_F(H5vccSchemeURLLoaderFactoryCacheBrowserTest,
                       SplashScreenShowsOwnChoiceIfPolicyCantRun) {
  base::HistogramTester histogram_tester;
  ShowSplashDomain();
  EXPECT_TRUE(NavigateToURL(shell(), GetSplashImageURL("&wasm_policy=true")));

  EXPECT_EQ("484x114", EvalJs(shell(), kSplashImageSizeScript));
  histogram_tester.ExpectTotalCount(kSplashWasmPolicyOutcome, 0);
  histogram_tester.ExpectTotalCount(kSplashWasmPolicyDecisionTime, 0);
  // The image, and splash_assets.json, which isn't found.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Cobalt.SplashScreen.FetchedFromCache"),
      base::BucketsAre(
          base::Bucket(SplashScreenFetchedState::kOkBuiltIn, 1),
          base::Bucket(SplashScreenFetchedState::kErrorOnResourceNotFound, 1)));
}

}  // namespace content

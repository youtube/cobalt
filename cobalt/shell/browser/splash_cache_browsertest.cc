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

#include "cobalt/shell/browser/splash_cache.h"

#include <memory>
#include <optional>
#include <string>

#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "cobalt/shell/browser/h5vcc_scheme_url_loader_factory.h"
#include "cobalt/shell/common/url_constants.h"
#include "cobalt/shell/embedded_resources/embedded_resources.h"
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

namespace {
const char kSafeSplashHtml[] = "<html><body>Safe</body></html>";
}  // namespace

// Clears the cached splash assets, as in Finch's crash safe mode.
class SplashCacheBrowserTest : public ContentBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());
    H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(
        embedded_test_server()->base_url().spec());
    // Navigates to the splash domain to access its caches.
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/title1.html")));
  }

  void TearDownOnMainThread() override {
    // Navigates away to ensure clean teardown.
    EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
    H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(nullptr);
    H5vccSchemeURLLoaderFactory::SetCachedSplashFileEnabledForTesting(
        std::nullopt);
    H5vccSchemeURLLoaderFactory::SetSplashDomainForTesting(std::nullopt);
    ContentBrowserTest::TearDownOnMainThread();
  }

  void ClearCachedSplashFiles() {
    base::RunLoop run_loop;
    content::ClearCachedSplashFiles(
        shell()->web_contents()->GetBrowserContext(),
        H5vccSchemeURLLoaderFactory::GetSplashDomain(), run_loop.QuitClosure());
    run_loop.Run();
  }

  bool HasCacheEntry(const std::string& cache_name,
                     const std::string& entry_name) {
    std::string script = base::StringPrintf(
        R"(
      (async () => {
        if (!(await caches.has('%s'))) {
          return false;
        }
        const cache = await caches.open('%s');
        return (await cache.match('%s')) !== undefined;
      })();
    )",
        cache_name.c_str(), cache_name.c_str(), entry_name.c_str());
    return EvalJs(shell(), script).ExtractBool();
  }

 protected:
  std::unique_ptr<GeneratedResourceMap> test_resource_map_;
};

IN_PROC_BROWSER_TEST_F(SplashCacheBrowserTest, DeletesCachedSplashFiles) {
  constexpr char kPopulateScript[] = R"(
    (async () => {
      try {
        const defaultCache = await caches.open('default');
        await defaultCache.put('splash.webm', new Response('bad_video'));
        await defaultCache.put('splash.png', new Response('bad_image'));
        await defaultCache.put('other.txt', new Response('keep_me'));
        const musicCache = await caches.open('music');
        await musicCache.put('splash.webm', new Response('bad_music_video'));
        await musicCache.put('splash.png', new Response('bad_music_image'));
        const otherCache = await caches.open('other');
        await otherCache.put('splash.webm', new Response('other_video'));
        return 'Success';
      } catch (e) {
        return 'Exception: ' + e.toString();
      }
    })();
  )";
  ASSERT_EQ("Success", EvalJs(shell(), kPopulateScript));

  base::HistogramTester histogram_tester;
  ClearCachedSplashFiles();

  histogram_tester.ExpectUniqueSample(
      "Cobalt.SplashScreen.ClearCachedFilesResult",
      SplashCacheClearResult::kCleared, 1);
  EXPECT_FALSE(HasCacheEntry("default", "splash.webm"));
  EXPECT_FALSE(HasCacheEntry("default", "splash.png"));
  EXPECT_FALSE(HasCacheEntry("music", "splash.webm"));
  EXPECT_FALSE(HasCacheEntry("music", "splash.png"));
  // Other entries and caches are kept.
  EXPECT_TRUE(HasCacheEntry("default", "other.txt"));
  EXPECT_TRUE(HasCacheEntry("other", "splash.webm"));
}

// Deletes the cached splash files that exist, even if some don't.
IN_PROC_BROWSER_TEST_F(SplashCacheBrowserTest, DeletesSomeCachedSplashFiles) {
  constexpr char kPopulateScript[] = R"(
    (async () => {
      const cache = await caches.open('music');
      await cache.put('splash.png', new Response('bad_music_image'));
      return 'Success';
    })();
  )";
  ASSERT_EQ("Success", EvalJs(shell(), kPopulateScript));

  base::HistogramTester histogram_tester;
  ClearCachedSplashFiles();

  histogram_tester.ExpectUniqueSample(
      "Cobalt.SplashScreen.ClearCachedFilesResult",
      SplashCacheClearResult::kCleared, 1);
  EXPECT_FALSE(HasCacheEntry("music", "splash.png"));
}

// Checking for the cached splash files doesn't create the caches.
IN_PROC_BROWSER_TEST_F(SplashCacheBrowserTest, NoCachedSplashFiles) {
  base::HistogramTester histogram_tester;
  ClearCachedSplashFiles();

  histogram_tester.ExpectUniqueSample(
      "Cobalt.SplashScreen.ClearCachedFilesResult",
      SplashCacheClearResult::kNoCachedFiles, 1);
  EXPECT_EQ("", EvalJs(shell(), "caches.keys().then((k) => k.join(','))"));
}

// After the cached splash files are deleted, the built-in ones are used, even
// with kEnableCachedSplashFile.
IN_PROC_BROWSER_TEST_F(SplashCacheBrowserTest, UsesBuiltInSplashAfterClear) {
  constexpr char kPopulateScript[] = R"(
    (async () => {
      const cache = await caches.open('default');
      await cache.put('splash.webm', new Response('bad_video'));
      return 'Success';
    })();
  )";
  ASSERT_EQ("Success", EvalJs(shell(), kPopulateScript));
  ClearCachedSplashFiles();

  // Uses a plain splash.html, which doesn't fetch the splash assets itself.
  test_resource_map_ = std::make_unique<GeneratedResourceMap>();
  LoaderEmbeddedResources::GenerateMap(*test_resource_map_);
  (*test_resource_map_)["splash.html"] = {
      reinterpret_cast<const unsigned char*>(kSafeSplashHtml),
      sizeof(kSafeSplashHtml) - 1};
  H5vccSchemeURLLoaderFactory::SetResourceMapForTesting(
      test_resource_map_.get());
  H5vccSchemeURLLoaderFactory::SetCachedSplashFileEnabledForTesting(true);
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(NavigateToURL(
      shell(), GURL(std::string(kH5vccEmbeddedScheme) + "://splash.html")));
  std::string fetch_script = base::StringPrintf(
      "fetch('%s://splash.webm').then((r) => r.ok)", kH5vccEmbeddedScheme);
  EXPECT_EQ(true, EvalJs(shell(), fetch_script));
  histogram_tester.ExpectUniqueSample("Cobalt.SplashScreen.FetchedFromCache",
                                      SplashScreenFetchedState::kOkBuiltIn, 1);
}

}  // namespace content

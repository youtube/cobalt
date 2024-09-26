// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "content/browser/loader/prefetch_browsertest_base.h"
#include "content/browser/web_package/mock_signed_exchange_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "net/base/features.h"
#include "net/base/filename_util.h"
#include "net/base/isolation_info.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/common/features.h"

namespace content {

class PrefetchBrowserTest
    : public PrefetchBrowserTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  PrefetchBrowserTest()
      : cross_origin_server_(std::make_unique<net::EmbeddedTestServer>()),
        signed_exchange_enabled_(std::get<0>(GetParam())),
        split_cache_enabled_(std::get<1>(GetParam())),
        split_cache_by_credentials_enabled_(std::get<2>(GetParam())) {}

  PrefetchBrowserTest(const PrefetchBrowserTest&) = delete;
  PrefetchBrowserTest& operator=(const PrefetchBrowserTest&) = delete;

  ~PrefetchBrowserTest() override = default;

  void SetUpOnMainThread() override {
    PrefetchBrowserTestBase::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  void SetUp() override {
    std::vector<base::test::FeatureRef> enable_features;
    std::vector<base::test::FeatureRef> disabled_features;

    (signed_exchange_enabled_ ? enable_features : disabled_features)
        .push_back(features::kSignedHTTPExchange);
    (split_cache_enabled_ ? enable_features : disabled_features)
        .push_back(net::features::kSplitCacheByNetworkIsolationKey);
    (split_cache_by_credentials_enabled_ ? enable_features : disabled_features)
        .push_back(net::features::kSplitCacheByIncludeCredentials);

    feature_list_.InitWithFeatures(enable_features, disabled_features);
    PrefetchBrowserTestBase::SetUp();
  }

 protected:
  std::unique_ptr<net::EmbeddedTestServer> cross_origin_server_;
  const bool signed_exchange_enabled_;
  const bool split_cache_enabled_;
  const bool split_cache_by_credentials_enabled_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

class PrefetchBrowserTestPrivacyChanges
    : public PrefetchBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  PrefetchBrowserTestPrivacyChanges()
      : privacy_changes_enabled_(GetParam()),
        cross_origin_server_(std::make_unique<net::EmbeddedTestServer>()) {}

  PrefetchBrowserTestPrivacyChanges(const PrefetchBrowserTestPrivacyChanges&) =
      delete;
  PrefetchBrowserTestPrivacyChanges& operator=(
      const PrefetchBrowserTestPrivacyChanges&) = delete;

  ~PrefetchBrowserTestPrivacyChanges() override = default;

  void SetUp() override {
    std::vector<base::test::FeatureRef> enable_features;
    std::vector<base::test::FeatureRef> disabled_features;
    if (privacy_changes_enabled_) {
      enable_features.push_back(blink::features::kPrefetchPrivacyChanges);
    } else {
      disabled_features.push_back(blink::features::kPrefetchPrivacyChanges);
    }
    feature_list_.InitWithFeatures(enable_features, disabled_features);
    PrefetchBrowserTestBase::SetUp();
  }

 protected:
  const bool privacy_changes_enabled_;
  std::unique_ptr<net::EmbeddedTestServer> cross_origin_server_;

 private:
  base::test::ScopedFeatureList feature_list_;
};

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_WIN)
// Test flakes on Fuchsia & Windows.
// TODO(crbug.com/1395163): Resolve flake and reenable.
#define MAYBE_RedirectNotFollowed DISABLED_RedirectNotFollowed
#else
#define MAYBE_RedirectNotFollowed RedirectNotFollowed
#endif
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTestPrivacyChanges,
                       MAYBE_RedirectNotFollowed) {
  const char* prefetch_path = "/prefetch.html";
  const char* redirect_path = "/redirect.html";
  const char* destination_path = "/destination.html";
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", redirect_path)));
  RegisterResponse(
      redirect_path,
      ResponseEntry("", "", {{"location", std::string(destination_path)}},
                    net::HTTP_MOVED_PERMANENTLY));
  RegisterResponse(destination_path,
                   ResponseEntry("<head><title>Prefetch Target</title></head>",
                                 "text/html", {{"cache-control", "no-store"}}));

  base::RunLoop prefetch_waiter;
  auto main_page_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), prefetch_path, &prefetch_waiter);
  auto destination_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), destination_path);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, main_page_counter->GetRequestCount());
  EXPECT_EQ(0, destination_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL destination_url = embedded_test_server()->GetURL(destination_path);
  // Loading a page that prefetches the redirect resource only follows the
  // redirect when the mode is follow.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, main_page_counter->GetRequestCount());

  NavigateToURLAndWaitTitle(destination_url, "Prefetch Target");
  const int expected_request_count = privacy_changes_enabled_ ? 1 : 2;
  EXPECT_EQ(expected_request_count, destination_counter->GetRequestCount());
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
}

// TODO(crbug.com/1414758): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginDocumentHasNoSameSiteCookies) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), target_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL("3p.example", target_path);
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' as='document' href='%s'></body>",
          cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  URLLoaderMonitor monitor({cross_origin_target_url});

  // Loading a page that prefetches the target URL would increment the
  // |request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  monitor.WaitForUrls();
  absl::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(cross_origin_target_url);
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->site_for_cookies.IsNull());
  ASSERT_TRUE(request->trusted_params);
  url::Origin cross_origin = url::Origin::Create(cross_origin_target_url);
  EXPECT_TRUE(net::IsolationInfo::Create(
                  net::IsolationInfo::RequestType::kOther, cross_origin,
                  cross_origin, net::SiteForCookies())
                  .IsEqualForTesting(request->trusted_params->isolation_info));
}

// TODO(crbug.com/1414758): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginDocumentReusedAsNavigation) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), target_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL("3p.example", target_path);
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' as='document' href='%s'></body>",
          cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the target URL would increment the
  // |request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the cross-origin target URL shouldn't hit the
  // network, and should be loaded from cache.
  NavigateToURLAndWaitTitle(cross_origin_target_url, "Prefetch Target");
}

// TODO(crbug.com/1414758): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginDocumentFromOpaqueOrigin) {
  // Prefetching as=document from a data: URL does not crash the renderer.
  EXPECT_TRUE(NavigateToURL(
      shell(),
      GURL("data:text/html,<title>Data URL Prefetch Target</title><link "
           "rel=prefetch as=document href=https://google.com>")));
}

// TODO(crbug.com/1414758): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(
    PrefetchBrowserTest,
    DISABLED_CrossOriginDocumentNotReusedAsNestedFrameNavigation) {
  // This test is relevant only with SplitCache.
  // TODO(crbug.com/910708): Remove this early-return when SplitCache is enabled
  // by default.
  if (!split_cache_enabled_)
    return;
  const char* prefetch_path = "/prefetch.html";
  const char* host_path = "/host.html";
  const char* iframe_path = "/iframe.html";
  RegisterResponse(
      host_path,
      ResponseEntry(base::StringPrintf(
          "<head><title>Cross-Origin Host</title></head><body><iframe "
          "onload='document.title=\"Host Loaded\"' src='%s'></iframe></body>",
          iframe_path)));
  RegisterResponse(iframe_path, ResponseEntry("<h1>I am an iframe</h1>"));

  base::RunLoop prefetch_waiter;
  auto cross_origin_iframe_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), iframe_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_host_url =
      cross_origin_server_->GetURL("3p.example", host_path);
  const GURL cross_origin_iframe_url =
      cross_origin_server_->GetURL("3p.example", iframe_path);
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' as='document' href='%s'></body>",
          cross_origin_iframe_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, cross_origin_iframe_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the cross-origin iframe URL increments its
  // counter.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, cross_origin_iframe_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Subsequent navigation to the cross-origin host site will trigger an iframe
  // load which will not reuse the iframe that was prefetched from
  // |prefetch_path|. This is because cross-origin document prefetches must
  // only be reused for top-level navigations, and cannot be reused as
  // cross-origin iframes.
  NavigateToURLAndWaitTitle(cross_origin_host_url, "Host Loaded");
  EXPECT_EQ(2, cross_origin_iframe_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());
}

// TODO(crbug.com/1414758): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginSubresourceNotReused) {
  // This test is relevant only with SplitCache.
  // TODO(crbug.com/910708): Remove this early-return when SplitCache is enabled
  // by default.
  if (!split_cache_enabled_)
    return;
  const char* prefetch_path = "/prefetch.html";
  const char* host_path = "/host.html";
  const char* subresource_path = "/subresource.js";
  RegisterResponse(
      host_path,
      ResponseEntry(base::StringPrintf(
          "<head><title>Cross-Origin Host</title></head><body><script src='%s' "
          "onload='document.title=\"Host Loaded\"'></script></body>",
          subresource_path)));
  RegisterResponse(subresource_path, ResponseEntry("console.log('I loaded')"));

  base::RunLoop prefetch_waiter;
  auto cross_origin_subresource_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), subresource_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_host_url =
      cross_origin_server_->GetURL("3p.example", host_path);
  const GURL cross_origin_subresource_url =
      cross_origin_server_->GetURL("3p.example", subresource_path);
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s'></body>",
                       cross_origin_subresource_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the cross-origin subresource URL
  // increments its counter.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Subsequent navigation to the cross-origin host attempting to reuse the
  // resource that was prefetched results in the request hitting the network.
  // This is because cross-origin subresources must only be reused within the
  // frame they were fetched from.
  NavigateToURLAndWaitTitle(cross_origin_host_url, "Host Loaded");
  EXPECT_EQ(2, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());
}

// TODO(crbug.com/1414758): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginSubresourceReusedByCurrentFrame) {
  const char* prefetch_path = "/prefetch.html";
  const char* use_prefetch_path = "/use-prefetch.html";
  const char* subresource_path = "/subresource.js";
  RegisterResponse(subresource_path, ResponseEntry("console.log('I loaded')"));

  base::RunLoop prefetch_waiter;
  auto cross_origin_subresource_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), subresource_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_subresource_url =
      cross_origin_server_->GetURL("3p.example", subresource_path);
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s'></body>",
                       cross_origin_subresource_url.spec().c_str())));
  RegisterResponse(use_prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><script src='%s' onload='document.title=\"Use "
                       "Prefetch Loaded\"'></script></body>",
                       cross_origin_subresource_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the cross-origin subresource URL
  // increments its counter.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shut down the cross-origin server.
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the same-origin document that attempts to reuse
  // the cross-origin prefetch is able to reuse the resource from the cache.
  NavigateToURLAndWaitTitle(embedded_test_server()->GetURL(use_prefetch_path),
                            "Use Prefetch Loaded");

  // Shutdown the same-origin server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
}

// This tests more of an implementation detail than anything. A single resource
// must be committed to the cache partition corresponding to a single
// NetworkAnonymizationKey. This means that even though it is considered "safe"
// to reused cross-origin subresource prefetches for top-level navigations, we
// can't actually do this, because the subresource is only reusable from the
// frame that fetched it.
// TODO(crbug.com/1414758): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginSubresourceNotReusedAsNavigation) {
  // This test is relevant only with SplitCache.
  // TODO(crbug.com/910708): Remove this early-return when SplitCache is enabled
  // by default.
  if (!split_cache_enabled_)
    return;
  const char* prefetch_path = "/prefetch.html";
  const char* subresource_path = "/subresource.js";
  RegisterResponse(subresource_path, ResponseEntry("console.log('I loaded');"));

  base::RunLoop prefetch_waiter;
  auto cross_origin_subresource_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), subresource_path, &prefetch_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_subresource_url =
      cross_origin_server_->GetURL("3p.example", subresource_path);
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s'></body>",
                       cross_origin_subresource_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the cross-origin subresource URL
  // increments its counter.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the same-origin server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  // Subsequent navigation to the cross-origin subresource itself will not be
  // reused from the cache, because the cached resource is not partitioned under
  // the cross-origin it is served from.
  EXPECT_TRUE(NavigateToURL(shell(), cross_origin_subresource_url));
  EXPECT_EQ(2, cross_origin_subresource_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the cross-origin server.
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, Simple) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_path)));
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), target_path, &prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  // Loading a page that prefetches the target URL would increment the
  // |request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_url, "Prefetch Target");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, DoublePrefetch) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  RegisterResponse(prefetch_path, ResponseEntry(base::StringPrintf(
                                      "<body><link rel='prefetch' href='%s'>"
                                      "<link rel='prefetch' href='%s'></body>",
                                      target_path, target_path)));
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), target_path, &prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  // Loading a page that prefetches the target URL would increment the
  // |request_counter|, but it should hit only once.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_url, "Prefetch Target");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, NoCacheAndNoStore) {
  const char* prefetch_path = "/prefetch.html";
  const char* nocache_path = "/target1.html";
  const char* nostore_path = "/target2.html";

  RegisterResponse(prefetch_path, ResponseEntry(base::StringPrintf(
                                      "<body>"
                                      "<link rel='prefetch' href='%s'>"
                                      "<link rel='prefetch' href='%s'></body>",
                                      nocache_path, nostore_path)));
  RegisterResponse(nocache_path,
                   ResponseEntry("<head><title>NoCache Target</title></head>",
                                 "text/html", {{"cache-control", "no-cache"}}));
  RegisterResponse(nostore_path,
                   ResponseEntry("<head><title>NoStore Target</title></head>",
                                 "text/html", {{"cache-control", "no-store"}}));

  base::RunLoop nocache_waiter;
  base::RunLoop nostore_waiter;
  auto nocache_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), nocache_path, &nocache_waiter);
  auto nostore_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), nostore_path, &nostore_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the target URL would increment the
  // fetch count for the both targets.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  nocache_waiter.Run();
  nostore_waiter.Run();
  EXPECT_EQ(1, nocache_request_counter->GetRequestCount());
  EXPECT_EQ(1, nostore_request_counter->GetRequestCount());
  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  // Subsequent navigation to the no-cache URL wouldn't hit the network, because
  // no-cache resource is kept available up to kPrefetchReuseMins.
  NavigateToURLAndWaitTitle(embedded_test_server()->GetURL(nocache_path),
                            "NoCache Target");
  EXPECT_EQ(1, nocache_request_counter->GetRequestCount());

  // Subsequent navigation to the no-store URL hit the network again, because
  // no-store resource is not cached even for prefetch.
  NavigateToURLAndWaitTitle(embedded_test_server()->GetURL(nostore_path),
                            "NoStore Target");
  EXPECT_EQ(2, nostore_request_counter->GetRequestCount());

  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, WithPreload) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_path = "/target.html";
  const char* preload_path = "/preload.js";
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_path)));
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "text/html",
                    {{"link", "</preload.js>;rel=\"preload\";as=\"script\""}}));
  RegisterResponse(preload_path,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  auto target_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), target_path);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), preload_path, &preload_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  preload_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  WaitUntilLoaded(embedded_test_server()->GetURL(preload_path));

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  NavigateToURLAndWaitTitle(target_url, "done");
}

// TODO(crbug.com/1414758): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginWithPreloadHasNoSameSiteCookies) {
  const char* target_path = "/target.html";
  const char* preload_path = "/preload.js";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "text/html",
                    {{"link", "</preload.js>;rel=\"preload\";as=\"script\""},
                     {"access-control-allow-origin", "*"}}));
  RegisterResponse(preload_path,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  auto target_request_counter =
      RequestCounter::CreateAndMonitor(cross_origin_server_.get(), target_path);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), preload_path, &preload_waiter);
  RegisterRequestHandler(cross_origin_server_.get());

  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL("3p.example", target_path);

  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s' as='document' "
                       "crossorigin='anonymous'></body>",
                       cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  URLLoaderMonitor monitor({cross_origin_target_url});

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  preload_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  GURL cross_origin_preload_url =
      cross_origin_server_->GetURL("3p.example", preload_path);
  WaitUntilLoaded(cross_origin_preload_url);

  monitor.WaitForUrls();
  absl::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(cross_origin_target_url);
  ASSERT_TRUE(request);
  ASSERT_TRUE(request->site_for_cookies.IsNull());
  ASSERT_TRUE(request->trusted_params);
  url::Origin cross_origin = url::Origin::Create(cross_origin_target_url);
  EXPECT_TRUE(net::IsolationInfo::Create(
                  net::IsolationInfo::RequestType::kOther, cross_origin,
                  cross_origin, net::SiteForCookies())
                  .IsEqualForTesting(request->trusted_params->isolation_info));
}

// Variants of this test:
// - PrefetchBrowserTest.CrossOriginWithPreloadAnonymous
// - PrefetchBrowserTest.CrossOriginWithPreloadCredentialled
// TODO(crbug.com/1414758): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginWithPreloadAnonymous) {
  const char* target_path = "/target.html";
  const char* preload_path = "/preload.js";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "text/html",
                    {{"link", "</preload.js>;rel=\"preload\";as=\"script\""},
                     {"access-control-allow-origin", "*"}}));
  RegisterResponse(preload_path,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  auto target_request_counter =
      RequestCounter::CreateAndMonitor(cross_origin_server_.get(), target_path);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), preload_path, &preload_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  base::RunLoop preload_waiter_second_request;
  auto preload_request_counter_second_request =
      RequestCounter::CreateAndMonitor(cross_origin_server_.get(), preload_path,
                                       &preload_waiter_second_request);

  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL("3p.example", target_path);

  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s' as='document' "
                       "crossorigin='anonymous'></body>",
                       cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  preload_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  GURL cross_origin_preload_url =
      cross_origin_server_->GetURL("3p.example", preload_path);
  WaitUntilLoaded(cross_origin_preload_url);

  // When SplitCache is enabled and the prefetch resource and its headers are
  // fetched with a modified NetworkAnonymizationKey, the preload header
  // resource must not be reusable by any other origin but its parent
  // prefetch's.
  // TODO(crbug.com/910708): When SplitCache is enabled by default, get rid of
  // the below conditional.
  if (split_cache_enabled_) {
    // Spin up another server, hosting a page with a preload header identical to
    // the one in |target_path|.
    const char* reuse_preload_attempt_path = "/reuse.html";
    RegisterResponse(
        reuse_preload_attempt_path,
        ResponseEntry(
            base::StringPrintf("<head><title>Other site</title><script "
                               "src='%s'></script></head>",
                               cross_origin_preload_url.spec().c_str()),
            "text/html",
            {{"link",
              base::StringPrintf("<%s>;rel=\"preload\";as=\"script\"",
                                 cross_origin_preload_url.spec().c_str())},
             {"access-control-allow-origin", "*"}}));
    std::unique_ptr<net::EmbeddedTestServer> other_cross_origin_server =
        std::make_unique<net::EmbeddedTestServer>();
    RegisterRequestHandler(other_cross_origin_server.get());

    ASSERT_TRUE(other_cross_origin_server->Start());

    // Navigate to a page on the above-created server. A request for the same
    // preload header fetched earlier must not be reusable, and must hit the
    // network.
    EXPECT_TRUE(NavigateToURL(
        shell(), other_cross_origin_server->GetURL(
                     "other3p.example", reuse_preload_attempt_path)));
    preload_waiter_second_request.Run();
    EXPECT_EQ(2, preload_request_counter_second_request->GetRequestCount());

    // We won't need this server again.
    EXPECT_TRUE(other_cross_origin_server->ShutdownAndWaitUntilComplete());
  } else if (split_cache_by_credentials_enabled_) {
    // The navigation is requested with credentials, but the preload is
    // requested anonymously. As a result of "SplitCacheByIncludeCredentials",
    // those aren't considered the same for the HTTP cache. Early return.
    // See the variant of this test in:
    // PrefetchBrowserTest.CrossOriginWithPreloadAnonymous
    return;
  }

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(cross_origin_target_url, "done");
}

// Variants of this test:
// - PrefetchBrowserTest.CrossOriginWithPreloadAnonymous
// - PrefetchBrowserTest.CrossOriginWithPreloadCredentialled
// TODO(crbug.com/1414758): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginWithPreloadCredentialled) {
  ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
  const auto port = embedded_test_server()->port();
  const char target_path[] = "/target.html";
  const char preload_path[] = "/preload.js";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title><script "
                    "src=\"./preload.js\"></script></head>",
                    "text/html",
                    {
                        {
                            "link",
                            "</preload.js>;rel=\"preload\";as=\"script\"",
                        },
                        {
                            "Access-Control-Allow-Origin",
                            "http://prefetch.com:" + base::NumberToString(port),
                        },
                        {
                            "Access-Control-Allow-Credentials",
                            "true",
                        },
                    }));
  RegisterResponse(preload_path,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  auto target_request_counter =
      RequestCounter::CreateAndMonitor(cross_origin_server_.get(), target_path);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), preload_path, &preload_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  base::RunLoop preload_waiter_second_request;
  auto preload_request_counter_second_request =
      RequestCounter::CreateAndMonitor(cross_origin_server_.get(), preload_path,
                                       &preload_waiter_second_request);

  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL cross_origin_target_url =
      cross_origin_server_->GetURL("3p.example", target_path);

  const char* prefetch_path = "/prefetch.html";
  RegisterResponse(prefetch_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s' as='document' "
                       "crossorigin='use-credentials'></body>",
                       cross_origin_target_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  embedded_test_server()->StartAcceptingConnections();
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("prefetch.com", prefetch_path)));
  preload_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  GURL cross_origin_preload_url =
      cross_origin_server_->GetURL("3p.example", preload_path);
  WaitUntilLoaded(cross_origin_preload_url);

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(cross_origin_target_url, "done");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, SignedExchangeWithPreload) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* preload_path_in_sxg = "/preload.js";

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML content
      // as "application/signed-exchange;v=b3".
      ResponseEntry(MockSignedExchangeHandler::kMockSxgPrefix +
                        "<head><title>Prefetch Target (SXG)</title><script "
                        "src=\"./preload.js\"></script></head>",
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"}}));
  RegisterResponse(preload_path_in_sxg,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  base::RunLoop prefetch_waiter;
  auto target_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), target_sxg_path, &prefetch_waiter);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), preload_path_in_sxg, &preload_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL preload_url_in_sxg =
      embedded_test_server()->GetURL(preload_path_in_sxg);
  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);

  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
      GURL(embedded_test_server()->GetURL(target_path)), "text/html",
      {{"Link", base::StringPrintf("<%s>;rel=\"preload\";as=\"script\"",
                                   preload_url_in_sxg.spec().c_str())}},
      net::SHA256HashValue({{0x00}}))});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());

  // Test after this point requires SignedHTTPExchange support
  if (!signed_exchange_enabled_)
    return;

  // If the header in the .sxg file is correctly extracted, we should
  // be able to also see the preload.
  preload_waiter.Run();
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());
  EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_sxg_url, "done");
}

// TODO(crbug.com/1414758): De-flake and re-enable.
IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest,
                       DISABLED_CrossOriginSignedExchangeWithPreload) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* preload_path_in_sxg = "/preload.js";

  RegisterResponse(
      target_sxg_path,
      // We mock the SignedExchangeHandler, so just return a HTML content
      // as "application/signed-exchange;v=b3".
      ResponseEntry(MockSignedExchangeHandler::kMockSxgPrefix +
                        "<head><title>Prefetch Target (SXG)</title><script "
                        "src=\"./preload.js\"></script></head>",
                    "application/signed-exchange;v=b3",
                    {{"x-content-type-options", "nosniff"}}));
  RegisterResponse(preload_path_in_sxg,
                   ResponseEntry("document.title=\"done\";", "text/javascript",
                                 {{"cache-control", "public, max-age=600"}}));

  base::RunLoop preload_waiter;
  base::RunLoop prefetch_waiter;
  auto target_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), target_sxg_path, &prefetch_waiter);
  auto preload_request_counter = RequestCounter::CreateAndMonitor(
      cross_origin_server_.get(), preload_path_in_sxg, &preload_waiter);
  RegisterRequestHandler(cross_origin_server_.get());
  ASSERT_TRUE(cross_origin_server_->Start());

  const GURL target_sxg_url =
      cross_origin_server_->GetURL("3p.example", target_sxg_path);
  const GURL preload_url_in_sxg =
      cross_origin_server_->GetURL("3p.example", preload_path_in_sxg);

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' as='document' href='%s'></body>",
          target_sxg_url.spec().c_str())));
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
      GURL(cross_origin_server_->GetURL("3p.example", target_path)),
      "text/html",
      {{"Link", base::StringPrintf("<%s>;rel=\"preload\";as=\"script\"",
                                   preload_url_in_sxg.spec().c_str())}},
      net::SHA256HashValue({{0x00}}))});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  // Loading a page that prefetches the target URL would increment both
  // |target_request_counter| and |preload_request_counter|.
  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  prefetch_waiter.Run();
  EXPECT_EQ(1, target_request_counter->GetRequestCount());

  // Test after this point requires SignedHTTPExchange support
  if (!signed_exchange_enabled_)
    return;
  // If the header in the .sxg file is correctly extracted, we should
  // be able to also see the preload.
  preload_waiter.Run();
  EXPECT_EQ(1, preload_request_counter->GetRequestCount());

  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  WaitUntilLoaded(preload_url_in_sxg);

  // Shutdown the servers.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(cross_origin_server_->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_sxg_url, "done");
}

IN_PROC_BROWSER_TEST_P(PrefetchBrowserTest, FileToHttp) {
  const char* target_path = "/target.html";
  RegisterResponse(
      target_path,
      ResponseEntry("<head><title>Prefetch Target</title></head>"));

  base::RunLoop prefetch_waiter;
  auto request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), target_path, &prefetch_waiter);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, request_counter->GetRequestCount());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_url = embedded_test_server()->GetURL(target_path);

  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath file_path = temp_dir.GetPath().AppendASCII("test.html");
    std::string file_content = base::StringPrintf(
        "<body><link rel='prefetch' as='document' href='%s'></body>",
        target_url.spec().c_str());
    ASSERT_TRUE(base::WriteFile(file_path, file_content));

    // Loading a page that prefetches the target URL would increment the
    // |request_counter|.
    GURL file_url = net::FilePathToFileURL(file_path);
    EXPECT_TRUE(NavigateToURL(shell(), file_url));
    prefetch_waiter.Run();
    EXPECT_EQ(1, request_counter->GetRequestCount());
    EXPECT_EQ(1, GetPrefetchURLLoaderCallCount());
  }

  // Shutdown the server.
  EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  NavigateToURLAndWaitTitle(target_url, "Prefetch Target");
}

INSTANTIATE_TEST_SUITE_P(PrefetchBrowserTest,
                         PrefetchBrowserTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

INSTANTIATE_TEST_SUITE_P(PrefetchBrowserTestPrivacyChanges,
                         PrefetchBrowserTestPrivacyChanges,
                         testing::Bool());

}  // namespace content

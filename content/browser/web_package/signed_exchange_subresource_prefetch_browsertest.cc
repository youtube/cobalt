// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time_override.h"
#include "build/build_config.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/loader/prefetch_browsertest_base.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_package/mock_signed_exchange_handler.h"
#include "content/browser/web_package/prefetched_signed_exchange_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/network_service_util.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_switches.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_cache.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "services/network/public/cpp/features.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/common/switches.h"

namespace content {

namespace {

std::string GetHeaderIntegrityString(const net::SHA256HashValue& hash) {
  std::string header_integrity_string = net::HashValue(hash).ToString();
  // Change "sha256/" to "sha256-".
  header_integrity_string[6] = '-';
  return header_integrity_string;
}

PrefetchedSignedExchangeCache::EntryMap GetCachedExchanges(Shell* shell) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell->web_contents()->GetPrimaryMainFrame());
  scoped_refptr<PrefetchedSignedExchangeCache> cache =
      rfh->EnsurePrefetchedSignedExchangeCache();
  PrefetchedSignedExchangeCache::EntryMap results;
  for (const auto& exchanges_it : cache->GetExchanges())
    results[exchanges_it.first] = exchanges_it.second->Clone();
  return results;
}

PrefetchBrowserTestBase::ResponseEntry CreateSignedExchangeResponseEntry(
    const std::string& content,
    const std::vector<std::pair<std::string, std::string>>& additional_headers =
        {}) {
  std::vector<std::pair<std::string, std::string>> headers = {
      {"x-content-type-options", "nosniff"}};

  for (const auto& additional_header : additional_headers) {
    headers.emplace_back(
        std::make_pair(additional_header.first, additional_header.second));
  }

  // We mock the SignedExchangeHandler, so just return the content as
  // "application/signed-exchange;v=b3".
  return PrefetchBrowserTestBase::ResponseEntry(
      MockSignedExchangeHandler::kMockSxgPrefix + content,
      "application/signed-exchange;v=b3", headers);
}

std::string CreateAlternateLinkHeader(const GURL& sxg_url,
                                      const GURL& inner_url) {
  return base::StringPrintf(
      "<%s>;"
      "rel=\"alternate\";"
      "type=\"application/signed-exchange;v=b3\";"
      "anchor=\"%s\"",
      sxg_url.spec().c_str(), inner_url.spec().c_str());
}

std::string CreateAllowedAltSxgLinkHeader(
    const GURL& inner_url,
    const net::SHA256HashValue& header_integrity) {
  return base::StringPrintf(
      "<%s>;rel=\"allowed-alt-sxg\";header-integrity=\"%s\"",
      inner_url.spec().c_str(),
      GetHeaderIntegrityString(header_integrity).c_str());
}

std::string CreatePreloadLinkHeader(const GURL& url, const char* as) {
  return base::StringPrintf("<%s>;rel=\"preload\";as=\"%s\"",
                            url.spec().c_str(), as);
}

// This class supplies a mock time which flies faster than the actual time
// using ScopedTimeClockOverrides. This is used to check the cache expiration
// logic.
class MockClock {
 public:
  // Returns a MockClock which will not be deleted. If the test use this class,
  // this method should be called once while single-threaded to initialize
  // ScopedTimeClockOverrides to avoid threading issues.
  static MockClock& Get() {
    static base::NoDestructor<MockClock> mock_clock;
    return *mock_clock;
  }

  MockClock(const MockClock&) = delete;
  MockClock& operator=(const MockClock&) = delete;

  void Advance(base::TimeDelta delta) {
    DCHECK_GE(delta, base::TimeDelta());
    base::AutoLock lock(lock_);
    offset_ += delta;
  }

 private:
  friend base::NoDestructor<MockClock>;

  static base::Time MockedNow() {
    return base::subtle::TimeNowIgnoringOverride() + mock_clock_->GetOffset();
  }

  MockClock() {
    mock_clock_ = this;
    time_override_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
        &MockClock::MockedNow, nullptr, nullptr);
  }

  ~MockClock() {}

  base::TimeDelta GetOffset() {
    base::AutoLock lock(lock_);
    return offset_;
  }

  static MockClock* mock_clock_;

  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_override_;
  base::Lock lock_;
  base::TimeDelta offset_ GUARDED_BY(lock_);
};

MockClock* MockClock::mock_clock_ = nullptr;

class NavigationHandleSXGAttributeObserver : public WebContentsObserver {
 public:
  explicit NavigationHandleSXGAttributeObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  NavigationHandleSXGAttributeObserver(
      const NavigationHandleSXGAttributeObserver&) = delete;
  NavigationHandleSXGAttributeObserver& operator=(
      const NavigationHandleSXGAttributeObserver&) = delete;

  // WebContentsObserver implementation.
  void ReadyToCommitNavigation(NavigationHandle* navigation_handle) override {
    had_prefetched_alt_sxg_ =
        navigation_handle->HasPrefetchedAlternativeSubresourceSignedExchange();
  }

  const absl::optional<bool>& had_prefetched_alt_sxg() const {
    return had_prefetched_alt_sxg_;
  }

 private:
  absl::optional<bool> had_prefetched_alt_sxg_;
};

}  // namespace

class SignedExchangePrefetchBrowserTest : public PrefetchBrowserTestBase {
 public:
  SignedExchangePrefetchBrowserTest() {
    // Call MockClock::Get() here to initialize ScopedMockClockOverride which
    // should be created while single-threaded.
    MockClock::Get();
  }

  SignedExchangePrefetchBrowserTest(const SignedExchangePrefetchBrowserTest&) =
      delete;
  SignedExchangePrefetchBrowserTest& operator=(
      const SignedExchangePrefetchBrowserTest&) = delete;

  ~SignedExchangePrefetchBrowserTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kSignedHTTPExchange);
    // Need to run the network service in process for testing cache expirity
    // (PrefetchMainResourceSXG_ExceedPrefetchReuseMins) using MockClock.
    ForceInProcessNetworkService();
    PrefetchBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    inactive_rfh_deletion_observer_ =
        std::make_unique<InactiveRenderFrameHostDeletionObserver>(
            shell()->web_contents());
    PrefetchBrowserTestBase::SetUpOnMainThread();
  }

 protected:
  static constexpr size_t kTestBlobStorageIPCThresholdBytes = 20;
  static constexpr size_t kTestBlobStorageMaxSharedMemoryBytes = 50;
  static constexpr size_t kTestBlobStorageMaxBlobMemorySize = 400;
  static constexpr uint64_t kTestBlobStorageMaxDiskSpace = 500;
  static constexpr uint64_t kTestBlobStorageMinFileSizeBytes = 10;
  static constexpr uint64_t kTestBlobStorageMaxFileSizeBytes = 100;

  std::unique_ptr<InactiveRenderFrameHostDeletionObserver>
      inactive_rfh_deletion_observer_;

  void SetBlobLimits() {
    scoped_refptr<ChromeBlobStorageContext> blob_context =
        ChromeBlobStorageContext::GetFor(
            shell()->web_contents()->GetBrowserContext());
    GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&SignedExchangePrefetchBrowserTest::SetBlobLimitsOnIO,
                       blob_context));
  }

  void RunPrefetchMainResourceSXGTest(const std::string& prefetch_page_hostname,
                                      const std::string& prefetch_page_path,
                                      const std::string& sxg_hostname,
                                      const std::string& sxg_path,
                                      const std::string& inner_url_hostname,
                                      const std::string& inner_url_path) {
    const net::SHA256HashValue header_integrity = {{0x01}};
    const std::string content =
        "<head><title>Prefetch Target (SXG)</title></head>";
    auto sxg_request_counter =
        RequestCounter::CreateAndMonitor(embedded_test_server(), sxg_path);

    LoadPrefetchMainResourceSXGTestPage(
        prefetch_page_hostname, prefetch_page_path, sxg_hostname, sxg_path,
        inner_url_hostname, inner_url_path, header_integrity, content,
        {} /* sxg_outer_headers */);

    EXPECT_EQ(1, sxg_request_counter->GetRequestCount());

    const GURL sxg_url = embedded_test_server()->GetURL(sxg_hostname, sxg_path);
    const GURL inner_url =
        embedded_test_server()->GetURL(inner_url_hostname, inner_url_path);

    const auto cached_exchanges = GetCachedExchanges(shell());
    EXPECT_EQ(1u, cached_exchanges.size());
    const auto it = cached_exchanges.find(sxg_url);
    ASSERT_TRUE(it != cached_exchanges.end());
    const std::unique_ptr<const PrefetchedSignedExchangeCacheEntry>& exchange =
        it->second;
    EXPECT_EQ(sxg_url, exchange->outer_url());
    EXPECT_EQ(inner_url, exchange->inner_url());
    EXPECT_EQ(header_integrity, *exchange->header_integrity());

    // Shutdown the server.
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

    base::HistogramTester histograms;
    // Subsequent navigation to the target URL wouldn't hit the network for
    // the target URL. The target content should still be read correctly.
    // The content is loaded from PrefetchedSignedExchangeCache.
    NavigateToURLAndWaitTitle(sxg_url, "Prefetch Target (SXG)");
    // Wait for the previous page's RFH to be deleted (if it changed) so that
    // the histograms will get updated.
    inactive_rfh_deletion_observer_->Wait();

    EXPECT_EQ(1, sxg_request_counter->GetRequestCount());
    histograms.ExpectBucketCount("PrefetchedSignedExchangeCache.Count", 1, 1);
  }

  void LoadPrefetchMainResourceSXGTestPage(
      const std::string& prefetch_page_hostname,
      const std::string& prefetch_page_path,
      const std::string& sxg_hostname,
      const std::string& sxg_path,
      const std::string& inner_url_hostname,
      const std::string& inner_url_path,
      const net::SHA256HashValue& header_integrity,
      const std::string& content,
      const std::vector<std::pair<std::string, std::string>>& sxg_outer_headers,
      const std::vector<std::pair<std::string, std::string>>&
          sxg_inner_headers = {},
      const base::Time& signature_expire_time = base::Time()) {
    auto sxg_request_counter =
        RequestCounter::CreateAndMonitor(embedded_test_server(), sxg_path);
    RegisterRequestHandler(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    const GURL prefetch_page_url = embedded_test_server()->GetURL(
        prefetch_page_hostname, prefetch_page_path);
    const GURL sxg_url = embedded_test_server()->GetURL(sxg_hostname, sxg_path);
    const GURL inner_url =
        embedded_test_server()->GetURL(inner_url_hostname, inner_url_path);

    RegisterResponse(prefetch_page_path,
                     ResponseEntry(base::StringPrintf(
                         "<body><link rel='prefetch' href='%s'></body>",
                         sxg_url.spec().c_str())));
    RegisterResponse(sxg_path, CreateSignedExchangeResponseEntry(
                                   content, sxg_outer_headers));

    MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
        sxg_url, SignedExchangeLoadResult::kSuccess, net::OK, inner_url,
        "text/html", sxg_inner_headers, header_integrity,
        signature_expire_time)});
    ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

    EXPECT_EQ(0, sxg_request_counter->GetRequestCount());

    EXPECT_TRUE(NavigateToURL(shell(), prefetch_page_url));

    WaitUntilLoaded(sxg_url);

    EXPECT_EQ(1, sxg_request_counter->GetRequestCount());
  }

 private:
  static void SetBlobLimitsOnIO(
      scoped_refptr<ChromeBlobStorageContext> context) {
    storage::BlobStorageLimits limits;
    limits.max_ipc_memory_size = kTestBlobStorageIPCThresholdBytes;
    limits.max_shared_memory_size = kTestBlobStorageMaxSharedMemoryBytes;
    limits.max_blob_in_memory_space = kTestBlobStorageMaxBlobMemorySize;
    limits.desired_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.effective_max_disk_space = kTestBlobStorageMaxDiskSpace;
    limits.min_page_file_size = kTestBlobStorageMinFileSizeBytes;
    limits.max_file_size = kTestBlobStorageMaxFileSizeBytes;
    context->context()->set_limits_for_testing(limits);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SignedExchangePrefetchBrowserTest,
                       PrefetchMainResourceSXG_SameOrigin) {
  RunPrefetchMainResourceSXGTest("example.com" /* prefetch_page_hostname */,
                                 "/prefetch.html" /* prefetch_page_path */,
                                 "example.com" /* sxg_hostname */,
                                 "/target.sxg" /* sxg_path */,
                                 "example.com" /* inner_url_hostname */,
                                 "/target.html" /* inner_url_path */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangePrefetchBrowserTest,
                       PrefetchMainResourceSXG_CrossOrigin) {
  RunPrefetchMainResourceSXGTest(
      "aggregator.example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "distoributor.example.com" /* sxg_hostname */,
      "/target.sxg" /* sxg_path */,
      "publisher.example.com" /* inner_url_hostname */,
      "/target.html" /* inner_url_path */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangePrefetchBrowserTest,
                       PrefetchMainResourceSXG_SameURL) {
  RunPrefetchMainResourceSXGTest("example.com" /* prefetch_page_hostname */,
                                 "/prefetch.html" /* prefetch_page_path */,
                                 "example.com" /* sxg_hostname */,
                                 "/target.html" /* sxg_path */,
                                 "example.com" /* inner_url_hostname */,
                                 "/target.html" /* inner_url_path */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangePrefetchBrowserTest,
                       PrefetchMainResourceSXG_BlobStorageLimit) {
  SetBlobLimits();

  std::string content = "<head><title>Prefetch Target (SXG)</title></head>";
  // Make the content larger than the disk space.
  content.resize(kTestBlobStorageMaxDiskSpace + 1, ' ');

  LoadPrefetchMainResourceSXGTestPage(
      "example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "example.com" /* sxg_hostname */, "/target.sxg" /* sxg_path */,
      "example.com" /* inner_url_hostname */,
      "/target.html" /* inner_url_path */,
      net::SHA256HashValue({{0x01}}) /* header_integrity */, content,
      {} /* sxg_outer_headers */, {} /* sxg_inner_headers */);
  const auto cached_exchanges = GetCachedExchanges(shell());
  // The content of prefetched SXG is larger than the Blob storage limit.
  // So the SXG should not be stored to the cache.
  EXPECT_EQ(0u, cached_exchanges.size());
}

IN_PROC_BROWSER_TEST_F(
    SignedExchangePrefetchBrowserTest,
    PrefetchMainResourceSXG_BlobStorageLimitWithContentLength) {
  // BlobBuilderFromStream's behavior is different when "content-length"
  // header is set. So we have BlobStorageLimit test with "content-length".
  SetBlobLimits();

  std::string content = "<head><title>Prefetch Target (SXG)</title></head>";
  // Make the content larger than the disk space.
  content.resize(kTestBlobStorageMaxDiskSpace + 1, ' ');
  LoadPrefetchMainResourceSXGTestPage(
      "example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "example.com" /* sxg_hostname */, "/target.sxg" /* sxg_path */,
      "example.com" /* inner_url_hostname */,
      "/target.html" /* inner_url_path */,
      net::SHA256HashValue({{0x01}}) /* header_integrity */, content,
      {} /* sxg_outer_headers */,
      {{"content-length",
        base::StringPrintf("%" PRIuS,
                           content.size())}} /* sxg_inner_headers */);
  const auto cached_exchanges = GetCachedExchanges(shell());
  // The content of prefetched SXG is larger than the Blob storage limit.
  // So the SXG should not be stored to the cache.
  EXPECT_EQ(0u, cached_exchanges.size());
}

IN_PROC_BROWSER_TEST_F(SignedExchangePrefetchBrowserTest,
                       PrefetchMainResourceSXG_CacheControlNoStore) {
  const std::string content =
      "<head><title>Prefetch Target (SXG)</title></head>";

  LoadPrefetchMainResourceSXGTestPage(
      "example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "example.com" /* sxg_hostname */, "/target.sxg" /* sxg_path */,
      "example.com" /* inner_url_hostname */,
      "/target.html" /* inner_url_path */,
      net::SHA256HashValue({{0x01}}) /* header_integrity */, content,
      {{"cache-control", "no-store"}} /* sxg_outer_headers */,
      {} /* sxg_inner_headers */);
  const auto cached_exchanges = GetCachedExchanges(shell());
  // The signed exchange which response header has "cache-control: no-store"
  // header should not be stored to the cache.
  EXPECT_EQ(0u, cached_exchanges.size());
}

IN_PROC_BROWSER_TEST_F(SignedExchangePrefetchBrowserTest,
                       PrefetchMainResourceSXG_VaryAsteriskHeader) {
  const std::string content =
      "<head><title>Prefetch Target (SXG)</title></head>";

  LoadPrefetchMainResourceSXGTestPage(
      "example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "example.com" /* sxg_hostname */, "/target.sxg" /* sxg_path */,
      "example.com" /* inner_url_hostname */,
      "/target.html" /* inner_url_path */,
      net::SHA256HashValue({{0x01}}) /* header_integrity */, content,
      {{"vary", "*"}} /* sxg_outer_headers */, {} /* sxg_inner_headers */);
  const auto cached_exchanges = GetCachedExchanges(shell());
  // The signed exchange which response header has "vary: *" header should not
  // be stored to the cache.
  EXPECT_EQ(0u, cached_exchanges.size());
}

IN_PROC_BROWSER_TEST_F(SignedExchangePrefetchBrowserTest,
                       PrefetchMainResourceSXG_VaryAcceptEncodingHeader) {
  const std::string content =
      "<head><title>Prefetch Target (SXG)</title></head>";

  LoadPrefetchMainResourceSXGTestPage(
      "example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "example.com" /* sxg_hostname */, "/target.sxg" /* sxg_path */,
      "example.com" /* inner_url_hostname */,
      "/target.html" /* inner_url_path */,
      net::SHA256HashValue({{0x01}}) /* header_integrity */, content,
      {{"vary", "accept-encoding"}} /* sxg_outer_headers */,
      {} /* sxg_inner_headers */);
  // The signed exchange which response header has "vary: accept-encoding"
  // header should be stored to the cache.
  const auto cached_exchanges = GetCachedExchanges(shell());
  EXPECT_EQ(1u, cached_exchanges.size());
}

IN_PROC_BROWSER_TEST_F(SignedExchangePrefetchBrowserTest,
                       PrefetchMainResourceSXG_ExceedPrefetchReuseMins) {
  const char* hostname = "example.com";
  const char* sxg_path = "/target.sxg";
  const char* inner_url_path = "/target.html";
  const std::string content =
      "<head><title>Prefetch Target (SXG)</title></head>";
  auto sxg_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), sxg_path);

  LoadPrefetchMainResourceSXGTestPage(
      hostname, "/prefetch.html" /* prefetch_page_path */, hostname, sxg_path,
      hostname, inner_url_path,
      net::SHA256HashValue({{0x01}}) /* header_integrity */, content,
      {} /* sxg_outer_headers */, {} /* sxg_inner_headers */);
  EXPECT_EQ(1, sxg_request_counter->GetRequestCount());

  const GURL sxg_url = embedded_test_server()->GetURL(hostname, sxg_path);
  const GURL inner_url =
      embedded_test_server()->GetURL(hostname, inner_url_path);

  EXPECT_EQ(1u, GetCachedExchanges(shell()).size());

  MockClock::Get().Advance(
      base::Seconds(net::HttpCache::kPrefetchReuseMins * 60 + 1));

  // Need to setup MockSignedExchangeHandlerFactory because the SXG is loaded
  // from the server again.
  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      sxg_url, SignedExchangeLoadResult::kSuccess, net::OK, inner_url,
      "text/html", {} /* sxg_inner_headers */,
      net::SHA256HashValue({{0x01}}) /* header_integrity */)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURLAndWaitTitle(sxg_url, "Prefetch Target (SXG)");

  // SXG must be fetched again.
  EXPECT_EQ(2, sxg_request_counter->GetRequestCount());
}

IN_PROC_BROWSER_TEST_F(SignedExchangePrefetchBrowserTest,
                       PrefetchMainResourceSXG_CacheControlPublic) {
  const char* hostname = "example.com";
  const char* sxg_path = "/target.sxg";
  const char* inner_url_path = "/target.html";
  const std::string content =
      "<head><title>Prefetch Target (SXG)</title></head>";
  auto sxg_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), sxg_path);
  const auto header_integrity = net::SHA256HashValue({{0x01}});

  LoadPrefetchMainResourceSXGTestPage(
      hostname, "/prefetch.html" /* prefetch_page_path */, hostname, sxg_path,
      hostname, inner_url_path, header_integrity, content,
      {{"cache-control",
        base::StringPrintf("public, max-age=%d",
                           net::HttpCache::kPrefetchReuseMins * 3 *
                               60)}} /* sxg_outer_headers */,
      {} /* sxg_inner_headers */);
  EXPECT_EQ(1, sxg_request_counter->GetRequestCount());

  const GURL sxg_url = embedded_test_server()->GetURL(hostname, sxg_path);
  const GURL inner_url =
      embedded_test_server()->GetURL(hostname, inner_url_path);

  EXPECT_EQ(1u, GetCachedExchanges(shell()).size());

  MockClock::Get().Advance(
      base::Seconds(net::HttpCache::kPrefetchReuseMins * 2 * 60));

  NavigateToURLAndWaitTitle(sxg_url, "Prefetch Target (SXG)");

  // SXG must Not be fetched again.
  EXPECT_EQ(1, sxg_request_counter->GetRequestCount());
}

IN_PROC_BROWSER_TEST_F(SignedExchangePrefetchBrowserTest,
                       PrefetchMainResourceSXG_CacheControlPublicExpire) {
  const char* hostname = "example.com";
  const char* sxg_path = "/target.sxg";
  const char* inner_url_path = "/target.html";
  const std::string content =
      "<head><title>Prefetch Target (SXG)</title></head>";
  auto sxg_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), sxg_path);

  LoadPrefetchMainResourceSXGTestPage(
      hostname, "/prefetch.html" /* prefetch_page_path */, hostname, sxg_path,
      hostname, inner_url_path,
      net::SHA256HashValue({{0x01}}) /* header_integrity */, content,
      {{"cache-control",
        base::StringPrintf("public, max-age=%d",
                           net::HttpCache::kPrefetchReuseMins * 3 *
                               60)}} /* sxg_outer_headers */,
      {} /* sxg_inner_headers */);
  EXPECT_EQ(1, sxg_request_counter->GetRequestCount());

  const GURL sxg_url = embedded_test_server()->GetURL(hostname, sxg_path);
  const GURL inner_url =
      embedded_test_server()->GetURL(hostname, inner_url_path);

  EXPECT_EQ(1u, GetCachedExchanges(shell()).size());

  MockClock::Get().Advance(
      base::Seconds(net::HttpCache::kPrefetchReuseMins * 3 * 60 + 1));

  // Need to setup MockSignedExchangeHandlerFactory because the SXG is loaded
  // from the server again.
  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      sxg_url, SignedExchangeLoadResult::kSuccess, net::OK, inner_url,
      "text/html", {} /* sxg_inner_headers */,
      net::SHA256HashValue({{0x01}}) /* header_integrity */)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURLAndWaitTitle(sxg_url, "Prefetch Target (SXG)");

  // SXG must be fetched again, because the cache has been expired.
  EXPECT_EQ(2, sxg_request_counter->GetRequestCount());
}

// Flaky on Linux TSan, http://crbug.com/1050879
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(THREAD_SANITIZER)
#define MAYBE_PrefetchMainResourceSXG_SignatureExpire \
  DISABLED_PrefetchMainResourceSXG_SignatureExpire
#else
#define MAYBE_PrefetchMainResourceSXG_SignatureExpire \
  PrefetchMainResourceSXG_SignatureExpire
#endif
IN_PROC_BROWSER_TEST_F(SignedExchangePrefetchBrowserTest,
                       MAYBE_PrefetchMainResourceSXG_SignatureExpire) {
  const char* hostname = "example.com";
  const char* sxg_path = "/target.sxg";
  const char* inner_url_path = "/target.html";
  const std::string content =
      "<head><title>Prefetch Target (SXG)</title></head>";
  auto sxg_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), sxg_path);

  LoadPrefetchMainResourceSXGTestPage(
      hostname, "/prefetch.html" /* prefetch_page_path */, hostname, sxg_path,
      hostname, inner_url_path,
      net::SHA256HashValue({{0x01}}) /* header_integrity */, content,
      {} /* sxg_outer_headers */, {} /* sxg_inner_headers */,
      base::Time::Now() +
          base::Minutes(net::HttpCache::kPrefetchReuseMins * 2));
  EXPECT_EQ(1, sxg_request_counter->GetRequestCount());

  const GURL sxg_url = embedded_test_server()->GetURL(hostname, sxg_path);
  const GURL inner_url =
      embedded_test_server()->GetURL(hostname, inner_url_path);

  EXPECT_EQ(1u, GetCachedExchanges(shell()).size());

  MockClock::Get().Advance(
      base::Minutes(net::HttpCache::kPrefetchReuseMins * 3));

  // Setup MockSignedExchangeHandlerFactory which triggers signature
  // verificvation error fallback.
  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      sxg_url, SignedExchangeLoadResult::kSignatureVerificationError,
      net::ERR_INVALID_SIGNED_EXCHANGE, inner_url, "",
      {} /* sxg_inner_headers */,
      net::SHA256HashValue({{0x01}}) /* header_integrity */)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  RegisterResponse(inner_url_path, ResponseEntry("<title>from server</title>"));

  NavigateToURLAndWaitTitle(sxg_url, "from server");

  // SXG must be fetched again, because:
  //  - The entry in PrefetchedSignedExchangeCache is expired (signature expire
  //    time).
  //  - The entry in HTTPCache is expired (more than kPrefetchReuseMins minutes
  //    passed). Note: The prefetched resource can skip cache revalidation for
  //    kPrefetchReuseMins minutes.
  EXPECT_EQ(2, sxg_request_counter->GetRequestCount());
}

IN_PROC_BROWSER_TEST_F(SignedExchangePrefetchBrowserTest, ClearAll) {
  const char* hostname = "example.com";
  const char* sxg_path = "/target.sxg";
  const char* inner_url_path = "/target.html";
  const std::string content =
      "<head><title>Prefetch Target (SXG)</title></head>";
  auto sxg_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), sxg_path);

  LoadPrefetchMainResourceSXGTestPage(
      hostname, "/prefetch.html" /* prefetch_page_path */, hostname, sxg_path,
      hostname, inner_url_path,
      net::SHA256HashValue({{0x01}}) /* header_integrity */, content,
      {} /* sxg_outer_headers */, {} /* sxg_inner_headers */);
  EXPECT_EQ(1, sxg_request_counter->GetRequestCount());

  const GURL sxg_url = embedded_test_server()->GetURL(hostname, sxg_path);
  const GURL inner_url =
      embedded_test_server()->GetURL(hostname, inner_url_path);

  EXPECT_EQ(1u, GetCachedExchanges(shell()).size());

  BrowsingDataRemover* remover =
      shell()->web_contents()->GetBrowserContext()->GetBrowsingDataRemover();
  BrowsingDataRemoverCompletionObserver completion_observer(remover);
  remover->RemoveAndReply(
      base::Time(), base::Time::Max(), BrowsingDataRemover::DATA_TYPE_CACHE,
      content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
      &completion_observer);
  completion_observer.BlockUntilCompletion();

  // Need to setup MockSignedExchangeHandlerFactory because the SXG is loaded
  // from the server again.
  MockSignedExchangeHandlerFactory factory({MockSignedExchangeHandlerParams(
      sxg_url, SignedExchangeLoadResult::kSuccess, net::OK, inner_url,
      "text/html", {} /* sxg_inner_headers */,
      net::SHA256HashValue({{0x01}}) /* header_integrity */)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  NavigateToURLAndWaitTitle(sxg_url, "Prefetch Target (SXG)");

  // SXG must be fetched again.
  EXPECT_EQ(2, sxg_request_counter->GetRequestCount());
}

class SignedExchangeSubresourcePrefetchBrowserTest
    : public PrefetchBrowserTestBase {
 public:
  SignedExchangeSubresourcePrefetchBrowserTest() {
    // Call MockClock::Get() here to initialize ScopedMockClockOverride which
    // should be created while single-threaded.
    MockClock::Get();
  }

  SignedExchangeSubresourcePrefetchBrowserTest(
      const SignedExchangeSubresourcePrefetchBrowserTest&) = delete;
  SignedExchangeSubresourcePrefetchBrowserTest& operator=(
      const SignedExchangeSubresourcePrefetchBrowserTest&) = delete;

  ~SignedExchangeSubresourcePrefetchBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PrefetchBrowserTestBase::SetUpCommandLine(command_line);
    // For window.gc().
    command_line->AppendSwitchASCII(blink::switches::kJavaScriptFlags,
                                    "--expose-gc");
  }

  void SetUp() override {
    std::vector<base::test::FeatureRef> enable_features;
    std::vector<base::test::FeatureRef> disabled_features;
    enable_features.push_back(features::kSignedHTTPExchange);
    // Needed for reporting test. Doesn't significantly impact other tests.
    enable_features.push_back(
        net::features::kPartitionNelAndReportingByNetworkIsolationKey);
    enable_features.push_back(
        net::features::kPartitionConnectionsByNetworkIsolationKey);
    enable_features.push_back(
        net::features::kPartitionSSLSessionsByNetworkIsolationKey);

    feature_list_.InitWithFeatures(enable_features, disabled_features);

    // Need to run the network service in process for testing cache expirity
    // (PrefetchMainResourceSXG_ExceedPrefetchReuseMins) using MockClock.
    ForceInProcessNetworkService();

    PrefetchBrowserTestBase::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    PrefetchBrowserTestBase::SetUpOnMainThread();
  }

 protected:
  // This test prefetches a main SXG which has appropriate subresrouce signed
  // exchange related headers, and navigate to the main SXG. If the subresource
  // signed exchange was prefetched and used, |expected_title| should be "done",
  // otherwise |expected_title| should be "from server" which is set by the
  // subresource script.
  // When |expected_script_fetch_count| is -1 this test doesn't check the script
  // fetch count.
  void RunPrefetchMainResourceSXGAndScriptSXGTest(
      const std::string& prefetch_page_hostname,
      const std::string& prefetch_page_path,
      const std::string& page_sxg_hostname,
      const std::string& page_sxg_path,
      const std::string& script_sxg_hostname,
      const std::string& script_sxg_path,
      const std::string& inner_url_hostname,
      const std::string& page_inner_url_path,
      const std::string& script_inner_url_path,
      const std::vector<std::pair<std::string, std::string>>&
          script_sxg_outer_headers,
      const std::string& additional_link_element_attributes,
      int64_t elapsed_time_after_prefetch,
      const std::string& expected_title,
      bool script_sxg_should_be_stored,
      int expected_script_fetch_count) {
    // When |expected_script_fetch_count| is -1, we don't check the script fetch
    // count.
    const bool check_script_fetch_count = expected_script_fetch_count != -1;

    auto script_sxg_request_counter = RequestCounter::CreateAndMonitor(
        embedded_test_server(), script_sxg_path);
    auto script_request_counter = RequestCounter::CreateAndMonitor(
        embedded_test_server(), script_inner_url_path);
    RegisterRequestHandler(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    const GURL prefetch_page_url = embedded_test_server()->GetURL(
        prefetch_page_hostname, prefetch_page_path);
    const GURL sxg_page_url =
        embedded_test_server()->GetURL(page_sxg_hostname, page_sxg_path);
    const GURL inner_url_page_url =
        embedded_test_server()->GetURL(inner_url_hostname, page_inner_url_path);
    const GURL sxg_script_url =
        embedded_test_server()->GetURL(script_sxg_hostname, script_sxg_path);
    const GURL inner_url_script_url = embedded_test_server()->GetURL(
        inner_url_hostname, script_inner_url_path);

    const net::SHA256HashValue page_header_integrity = {{0x01}};
    const net::SHA256HashValue script_header_integrity = {{0x02}};

    const std::string outer_link_header =
        CreateAlternateLinkHeader(sxg_script_url, inner_url_script_url);
    const std::string inner_link_headers =
        base::JoinString(
            {CreateAllowedAltSxgLinkHeader(inner_url_script_url,
                                           script_header_integrity),
             CreatePreloadLinkHeader(inner_url_script_url, "script")},
            ",");

    RegisterResponse(prefetch_page_path,
                     ResponseEntry(base::StringPrintf(
                         "<body><link rel='prefetch' href='%s' %s></body>",
                         sxg_page_url.spec().c_str(),
                         additional_link_element_attributes.c_str())));
    RegisterResponse(
        script_inner_url_path,
        ResponseEntry("document.title=\"from server\";", "text/javascript",
                      {{"cache-control", "public, max-age=600"}}));
    RegisterResponse(page_sxg_path,
                     CreateSignedExchangeResponseEntry(
                         "<head><title>Prefetch Target (SXG)</title>"
                         "<script src=\"./script.js\"></script></head>",
                         {{"link", outer_link_header},
                          {"cache-control",
                           base::StringPrintf(
                               "public, max-age=%d",
                               net::HttpCache::kPrefetchReuseMins * 3 * 60)}}));
    RegisterResponse(script_sxg_path,
                     CreateSignedExchangeResponseEntry(
                         "document.title=\"done\";", script_sxg_outer_headers));
    MockSignedExchangeHandlerFactory factory(
        {MockSignedExchangeHandlerParams(
             sxg_page_url, SignedExchangeLoadResult::kSuccess, net::OK,
             inner_url_page_url, "text/html", {{"Link", inner_link_headers}},
             page_header_integrity),
         MockSignedExchangeHandlerParams(
             sxg_script_url, SignedExchangeLoadResult::kSuccess, net::OK,
             inner_url_script_url, "text/javascript", {},
             script_header_integrity)});
    ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

    EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());
    EXPECT_TRUE(NavigateToURL(shell(), prefetch_page_url));

    WaitUntilLoaded(sxg_page_url);
    WaitUntilLoaded(sxg_script_url);

    EXPECT_EQ(1, script_sxg_request_counter->GetRequestCount());
    if (check_script_fetch_count) {
      EXPECT_EQ(0, script_request_counter->GetRequestCount());
    }
    EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

    const auto cached_exchanges = GetCachedExchanges(shell());
    EXPECT_EQ(script_sxg_should_be_stored ? 2u : 1u, cached_exchanges.size());

    const auto target_it = cached_exchanges.find(sxg_page_url);
    ASSERT_TRUE(target_it != cached_exchanges.end());
    EXPECT_EQ(sxg_page_url, target_it->second->outer_url());
    EXPECT_EQ(inner_url_page_url, target_it->second->inner_url());
    EXPECT_EQ(page_header_integrity, *target_it->second->header_integrity());

    if (script_sxg_should_be_stored) {
      const auto script_it = cached_exchanges.find(sxg_script_url);
      ASSERT_TRUE(script_it != cached_exchanges.end());
      EXPECT_EQ(sxg_script_url, script_it->second->outer_url());
      EXPECT_EQ(inner_url_script_url, script_it->second->inner_url());
      EXPECT_EQ(script_header_integrity,
                *script_it->second->header_integrity());
    }

    MockClock::Get().Advance(base::Seconds(elapsed_time_after_prefetch));

    // Subsequent navigation to the target URL wouldn't hit the network for
    // the target URL. The target content should still be read correctly.
    // The content is loaded from PrefetchedSignedExchangeCache.
    //
    // When |expected_title| == "done":
    //   The script is also loaded from PrefetchedSignedExchangeCache.
    // When |expected_title| == "from server":
    //   The script is loaded from the server.
    NavigateToURLAndWaitTitle(sxg_page_url, expected_title);

    EXPECT_EQ(1, script_sxg_request_counter->GetRequestCount());
    if (check_script_fetch_count) {
      EXPECT_EQ(expected_script_fetch_count,
                script_request_counter->GetRequestCount());
    }
  }

  // Test that CORB logic works well with prefetched signed exchange
  // subresources. This test loads a main SXG which signed by
  // "publisher.example.com", and loads a SXG subresource using a <script> tag.
  //
  // When |cross_origin| is set, the SXG subresource is signed by
  // "crossorigin.example.com", otherwise sined by "publisher.example.com".
  // |content| is the inner body content of the SXG subresource.
  // |mime_type| is the MIME tyepe of the inner response of the SXG subresource.
  // When |has_nosniff| is set, the inner response header of the SXG subresource
  // has "x-content-type-options: nosniff" header.
  // |corb_allowed| is whether we expect CORB/ORB to allow this resource. We
  // will check this based on the document title.
  void RunCrossOriginReadBlockingTest(bool cross_origin,
                                      const std::string& content,
                                      const std::string& mime_type,
                                      bool has_nosniff,
                                      bool corb_allowed) {
    const char* prefetch_path = "/prefetch.html";
    const char* target_sxg_path = "/target.sxg";
    const char* target_path = "/target.html";
    const char* script_sxg_path = "/script_js.sxg";
    const char* script_path = "/script.js";

    // If CORB/ORB blocks the resource, it will retain its original title,
    // "original title". If the resource is allowed, it will modify the title
    // to "done".
    const char* expected_title = corb_allowed ? "done" : "original title";

    auto script_sxg_request_counter = RequestCounter::CreateAndMonitor(
        embedded_test_server(), script_sxg_path);
    auto script_request_counter =
        RequestCounter::CreateAndMonitor(embedded_test_server(), script_path);
    RegisterRequestHandler(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());

    const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
    const GURL target_url =
        embedded_test_server()->GetURL("publisher.example.com", target_path);
    const GURL script_sxg_url = embedded_test_server()->GetURL(script_sxg_path);
    const GURL script_url = embedded_test_server()->GetURL(
        cross_origin ? "crossorigin.example.com" : "publisher.example.com",
        script_path);

    const net::SHA256HashValue target_header_integrity = {{0x01}};
    const net::SHA256HashValue script_header_integrity = {{0x02}};

    const std::string outer_link_header =
        CreateAlternateLinkHeader(script_sxg_url, script_url);
    const std::string inner_link_headers =
        base::JoinString(
            {CreateAllowedAltSxgLinkHeader(script_url, script_header_integrity),
             CreatePreloadLinkHeader(script_url, "script")},
            ",");
    RegisterResponse(
        prefetch_path,
        ResponseEntry(base::StringPrintf(
            "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
    RegisterResponse(
        script_path,
        ResponseEntry("document.title=\"from server\";", "text/javascript"));
    RegisterResponse(target_sxg_path,
                     CreateSignedExchangeResponseEntry(
                         base::StringPrintf("<head><script src=\"%s\"></script>"
                                            "<title>original title</title>"
                                            "</head>",
                                            script_url.spec().c_str()),
                         {{"link", outer_link_header}}));
    RegisterResponse(script_sxg_path,
                     CreateSignedExchangeResponseEntry(content));
    std::vector<std::pair<std::string, std::string>>
        script_inner_response_headers;
    if (has_nosniff) {
      script_inner_response_headers.emplace_back(std::make_pair(
          std::string("x-content-type-options"), std::string("nosniff")));
    }
    MockSignedExchangeHandlerFactory factory(
        {MockSignedExchangeHandlerParams(
             target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
             target_url, "text/html", {{"Link", inner_link_headers}},
             target_header_integrity),
         MockSignedExchangeHandlerParams(
             script_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
             script_url, mime_type, script_inner_response_headers,
             script_header_integrity)});
    ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

    WaitUntilLoaded(target_sxg_url);
    WaitUntilLoaded(script_sxg_url);

    EXPECT_EQ(1, script_sxg_request_counter->GetRequestCount());
    EXPECT_EQ(0, script_request_counter->GetRequestCount());

    EXPECT_EQ(2u, GetCachedExchanges(shell()).size());

    NavigateToURLAndWaitTitle(target_sxg_url, expected_title);

    EXPECT_EQ(1, script_sxg_request_counter->GetRequestCount());
    EXPECT_EQ(0, script_request_counter->GetRequestCount());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       MainResourceSXGAndScriptSXG_SameOrigin) {
  RunPrefetchMainResourceSXGAndScriptSXGTest(
      "example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "example.com" /* page_sxg_hostname */, "/target.sxg" /* page_sxg_path */,
      "example.com" /* script_sxg_hostname */,
      "/script.sxg" /* script_sxg_path */,
      "example.com" /* inner_url_hostname */,
      "/target.html" /* page_inner_url_path */,
      "/script.js" /* script_inner_url_path */,
      {} /* script_sxg_outer_headers */,
      "" /* additional_link_element_attributes */,
      0 /* elapsed_time_after_prefetch */, "done" /* expected_title */,
      true /* script_sxg_should_be_stored */,
      0 /* expected_script_fetch_count */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       MainResourceSXGAndScriptSXG_SameUrl) {
  // This test checks the behavior of subresource signed exchange prefetching
  // when the inner URL and the outer URL of signed exchanges are same.
  RunPrefetchMainResourceSXGAndScriptSXGTest(
      "example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "example.com" /* page_sxg_hostname */, "/target.html" /* page_sxg_path */,
      "example.com" /* script_sxg_hostname */,
      "/script.js" /* script_sxg_path */,
      "example.com" /* inner_url_hostname */,
      "/target.html" /* page_inner_url_path */,
      "/script.js" /* script_inner_url_path */,
      {} /* script_sxg_outer_headers */,
      "" /* additional_link_element_attributes */,
      0 /* elapsed_time_after_prefetch */, "done" /* expected_title */,
      true /* script_sxg_should_be_stored */,
      // Note that we don't check the script fetch count in this test.
      -1 /* expected_script_fetch_count  */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       MainResourceSXGAndScriptSXG_CrossOrigin) {
  // This test checks the behavior of subresource signed exchange prefetching
  // when the inner URL's origin (= publisher) and the outer URL's origin
  // (= distoributor) of signed exchanges is different from the prefetching page
  // (= aggregator).
  RunPrefetchMainResourceSXGAndScriptSXGTest(
      "aggregator.example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "distoributor.example.com" /* page_sxg_hostname */,
      "/target.sxg" /* page_sxg_path */,
      "distoributor.example.com" /* script_sxg_hostname */,
      "/script.sxg" /* script_sxg_path */,
      "publisher.example.com" /* inner_url_hostname */,
      "/target.html" /* page_inner_url_path */,
      "/script.js" /* script_inner_url_path */,
      {{"Access-Control-Allow-Origin", "*"}} /* script_sxg_outer_headers */,
      "" /* additional_link_element_attributes */,
      0 /* elapsed_time_after_prefetch */, "done" /* expected_title */,
      true /* script_sxg_should_be_stored */,
      0 /* expected_script_fetch_count */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       MainResourceSXGAndScriptSXG_CrossOrigin_CorsError) {
  // This test is similar to MainResourceSXGAndScriptSXG_CrossOrigin, but the
  // script SXG is served without Access-Control-Allow-Origin. Prefetch of the
  // script SXG should fail with CORS error.
  RunPrefetchMainResourceSXGAndScriptSXGTest(
      "aggregator.example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "distoributor.example.com" /* page_sxg_hostname */,
      "/target.sxg" /* page_sxg_path */,
      "distoributor.example.com" /* script_sxg_hostname */,
      "/script.sxg" /* script_sxg_path */,
      "publisher.example.com" /* inner_url_hostname */,
      "/target.html" /* page_inner_url_path */,
      "/script.js" /* script_inner_url_path */,
      {} /* script_sxg_outer_headers */,
      "" /* additional_link_element_attributes */,
      0 /* elapsed_time_after_prefetch */, "from server" /* expected_title */,
      false /* script_sxg_should_be_stored */,
      1 /* expected_script_fetch_count */);
}

// Flaky. See https://crbug.com/1415204 and https://crbug.com/1494880.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#define MAYBE_MainResourceSXGAndScriptSXG_CrossOrigin_AsDocument \
  DISABLED_MainResourceSXGAndScriptSXG_CrossOrigin_AsDocument
#else
#define MAYBE_MainResourceSXGAndScriptSXG_CrossOrigin_AsDocument \
  MainResourceSXGAndScriptSXG_CrossOrigin_AsDocument
#endif
IN_PROC_BROWSER_TEST_F(
    SignedExchangeSubresourcePrefetchBrowserTest,
    MAYBE_MainResourceSXGAndScriptSXG_CrossOrigin_AsDocument) {
  // This test is almost same as MainResourceSXGAndScriptSXG_CrossOrigin. The
  // only difference is that the <link rel=prefetch> element has "as=document"
  // attribute which was introduced to support cross origin prefetch with
  // SplitCacheByNetworkIsolationKey feature. Note that even if
  // SplitCacheByNetworkIsolationKey feature is enabled, current Chromium
  // implementation doesn't require as=document for prefetching main resource
  // and subresource signed exchanges.
  RunPrefetchMainResourceSXGAndScriptSXGTest(
      "aggregator.example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "distoributor.example.com" /* page_sxg_hostname */,
      "/target.sxg" /* page_sxg_path */,
      "distoributor.example.com" /* script_sxg_hostname */,
      "/script.sxg" /* script_sxg_path */,
      "publisher.example.com" /* inner_url_hostname */,
      "/target.html" /* page_inner_url_path */,
      "/script.js" /* script_inner_url_path */,
      {{"Access-Control-Allow-Origin", "*"}} /* script_sxg_outer_headers */,
      "as='document'" /* additional_link_element_attributes */,
      0 /* elapsed_time_after_prefetch */, "done" /* expected_title */,
      true /* script_sxg_should_be_stored */,
      0 /* expected_script_fetch_count */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       MainResourceSXGAndScriptSXG_DifferentOriginScriptSXG) {
  // When the subresource SXG was served from different origin from the main
  // SXG, the subresource should be loaded from the server.
  RunPrefetchMainResourceSXGAndScriptSXGTest(
      "aggregator.example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "distoributor1.example.com" /* page_sxg_hostname */,
      "/target.sxg" /* page_sxg_path */,
      "distoributor2.example.com" /* script_sxg_hostname */,
      "/script.sxg" /* script_sxg_path */,
      "publisher.example.com" /* inner_url_hostname */,
      "/target.html" /* page_inner_url_path */,
      "/script.js" /* script_inner_url_path */,
      {{"Access-Control-Allow-Origin", "*"}} /* script_sxg_outer_headers */,
      "" /* additional_link_element_attributes */,
      0 /* elapsed_time_after_prefetch */, "from server" /* expected_title */,
      true /* script_sxg_should_be_stored */,
      1 /* expected_script_fetch_count */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       MainResourceSXGAndScriptSXG_CacheControlNoStore) {
  RunPrefetchMainResourceSXGAndScriptSXGTest(
      "example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "example.com" /* page_sxg_hostname */, "/target.sxg" /* page_sxg_path */,
      "example.com" /* script_sxg_hostname */,
      "/script.sxg" /* script_sxg_path */,
      "example.com" /* inner_url_hostname */,
      "/target.html" /* page_inner_url_path */,
      "/script.js" /* script_inner_url_path */,
      {{"cache-control", "no-store"}} /* script_sxg_outer_headers */,
      "" /* additional_link_element_attributes */,
      0 /* elapsed_time_after_prefetch */, "from server" /* expected_title */,
      false /* script_sxg_should_be_stored */,
      1 /* expected_script_fetch_count */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       MainResourceSXGAndScriptSXG_VaryAsteriskHeader) {
  RunPrefetchMainResourceSXGAndScriptSXGTest(
      "example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "example.com" /* page_sxg_hostname */, "/target.sxg" /* page_sxg_path */,
      "example.com" /* script_sxg_hostname */,
      "/script.sxg" /* script_sxg_path */,
      "example.com" /* inner_url_hostname */,
      "/target.html" /* page_inner_url_path */,
      "/script.js" /* script_inner_url_path */,
      {{"vary", "*"}} /* script_sxg_outer_headers */,
      "" /* additional_link_element_attributes */,
      0 /* elapsed_time_after_prefetch */, "from server" /* expected_title */,
      false /* script_sxg_should_be_stored */,
      1 /* expected_script_fetch_count */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       MainResourceSXGAndScriptSXG_VaryAcceptEncodingHeader) {
  RunPrefetchMainResourceSXGAndScriptSXGTest(
      "example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "example.com" /* page_sxg_hostname */, "/target.sxg" /* page_sxg_path */,
      "example.com" /* script_sxg_hostname */,
      "/script.sxg" /* script_sxg_path */,
      "example.com" /* inner_url_hostname */,
      "/target.html" /* page_inner_url_path */,
      "/script.js" /* script_inner_url_path */,
      {{"vary", "accept-encoding"}} /* script_sxg_outer_headers */,
      "" /* additional_link_element_attributes */,
      0 /* elapsed_time_after_prefetch */, "done" /* expected_title */,
      true /* script_sxg_should_be_stored */,
      0 /* expected_script_fetch_count */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       MainResourceSXGAndScriptSXG_ExceedPrefetchReuseMins) {
  RunPrefetchMainResourceSXGAndScriptSXGTest(
      "example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "example.com" /* page_sxg_hostname */, "/target.sxg" /* page_sxg_path */,
      "example.com" /* script_sxg_hostname */,
      "/script.sxg" /* script_sxg_path */,
      "example.com" /* inner_url_hostname */,
      "/target.html" /* page_inner_url_path */,
      "/script.js" /* script_inner_url_path */,
      {} /* script_sxg_outer_headers */,
      "" /* additional_link_element_attributes */,
      net::HttpCache::kPrefetchReuseMins * 60 + 1
      /* elapsed_time_after_prefetch */,
      "from server" /* expected_title */,
      true /* script_sxg_should_be_stored */,
      1 /* expected_script_fetch_count */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       MainResourceSXGAndScriptSXG_CacheControlPublic) {
  RunPrefetchMainResourceSXGAndScriptSXGTest(
      "example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "example.com" /* page_sxg_hostname */, "/target.sxg" /* page_sxg_path */,
      "example.com" /* script_sxg_hostname */,
      "/script.sxg" /* script_sxg_path */,
      "example.com" /* inner_url_hostname */,
      "/target.html" /* page_inner_url_path */,
      "/script.js" /* script_inner_url_path */,
      {{"cache-control",
        base::StringPrintf("public, max-age=%d",
                           net::HttpCache::kPrefetchReuseMins * 3 *
                               60)}} /* script_sxg_outer_headers */,
      "" /* additional_link_element_attributes */,
      net::HttpCache::kPrefetchReuseMins * 2 * 60
      /* elapsed_time_after_prefetch */,
      "done" /* expected_title */, true /* script_sxg_should_be_stored */,
      0 /* expected_script_fetch_count */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       MainResourceSXGAndScriptSXG_CacheControlPublicExpire) {
  RunPrefetchMainResourceSXGAndScriptSXGTest(
      "example.com" /* prefetch_page_hostname */,
      "/prefetch.html" /* prefetch_page_path */,
      "example.com" /* page_sxg_hostname */, "/target.sxg" /* page_sxg_path */,
      "example.com" /* script_sxg_hostname */,
      "/script.sxg" /* script_sxg_path */,
      "example.com" /* inner_url_hostname */,
      "/target.html" /* page_inner_url_path */,
      "/script.js" /* script_inner_url_path */,
      {{"cache-control",
        base::StringPrintf("public, max-age=%d",
                           net::HttpCache::kPrefetchReuseMins * 3 *
                               60)}} /* script_sxg_outer_headers */,
      "" /* additional_link_element_attributes */,
      net::HttpCache::kPrefetchReuseMins * 3 * 60 + 1
      /* elapsed_time_after_prefetch */,
      "from server" /* expected_title */,
      true /* script_sxg_should_be_stored */,
      1 /* expected_script_fetch_count */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       ImageSrcsetAndSizes) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* image1_sxg_path = "/image1_png.sxg";
  const char* image1_path = "/image1.png";
  const char* image2_sxg_path = "/image2_png.sxg";
  const char* image2_path = "/image2.png";

  auto image1_sxg_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), image1_sxg_path);
  auto image2_sxg_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), image2_sxg_path);
  auto image1_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), image1_path);
  auto image2_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), image2_path);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  const GURL image1_sxg_url = embedded_test_server()->GetURL(image1_sxg_path);
  const GURL image1_url = embedded_test_server()->GetURL(image1_path);
  const GURL image2_sxg_url = embedded_test_server()->GetURL(image2_sxg_path);
  const GURL image2_url = embedded_test_server()->GetURL(image2_path);

  std::string image_contents;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::FilePath path;
    ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &path));
    path = path.AppendASCII("loader/empty16x16.png");
    ASSERT_TRUE(base::PathExists(path));
    ASSERT_TRUE(base::ReadFileToString(path, &image_contents));
  }

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue image1_header_integrity = {{0x02}};
  const net::SHA256HashValue image2_header_integrity = {{0x03}};

  const std::string outer_link_header =
      base::JoinString({CreateAlternateLinkHeader(image1_sxg_url, image1_url),
                        CreateAlternateLinkHeader(image2_sxg_url, image2_url)},
                       ",");
  const std::string inner_link_headers =
      base::JoinString(
          {CreateAllowedAltSxgLinkHeader(image1_url, image1_header_integrity),
           CreateAllowedAltSxgLinkHeader(image2_url, image2_header_integrity),
           base::StringPrintf("<%s>;rel=\"preload\";as=\"image\";"
                              // imagesrcset says the size of image1 is 320, and
                              // the size of image2 is 160.
                              "imagesrcset=\"%s 320w, %s 160w\";"
                              // imagesizes says the size of the image is 320.
                              // So image1 is selected.
                              "imagesizes=\"320px\"",
                              image2_url.spec().c_str(),
                              image1_url.spec().c_str(),
                              image2_url.spec().c_str())},
          ",");
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(image1_path, ResponseEntry(image_contents, "image/png"));
  RegisterResponse(image2_path, ResponseEntry(image_contents, "image/png"));
  RegisterResponse(target_sxg_path,
                   CreateSignedExchangeResponseEntry(
                       base::StringPrintf(
                           "<head>"
                           "<title>Prefetch Target (SXG)</title>"
                           "</head>"
                           "<img src=\"%s\" onload=\"document.title='done'\">",
                           image1_url.spec().c_str()),
                       {{"link", outer_link_header}}));
  RegisterResponse(image1_sxg_path,
                   CreateSignedExchangeResponseEntry(image_contents));
  MockSignedExchangeHandlerFactory factory(
      {MockSignedExchangeHandlerParams(
           target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           target_url, "text/html", {{"Link", inner_link_headers}},
           target_header_integrity),
       MockSignedExchangeHandlerParams(
           image1_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           image1_url, "image/png", {}, image1_header_integrity),
       MockSignedExchangeHandlerParams(
           image2_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           image2_url, "image/png", {}, image2_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));
  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(image1_sxg_url);

  EXPECT_EQ(1, image1_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, image1_request_counter->GetRequestCount());
  EXPECT_EQ(0, image2_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, image2_request_counter->GetRequestCount());

  const auto cached_exchanges = GetCachedExchanges(shell());
  EXPECT_EQ(2u, cached_exchanges.size());

  const auto target_it = cached_exchanges.find(target_sxg_url);
  ASSERT_TRUE(target_it != cached_exchanges.end());
  EXPECT_EQ(target_sxg_url, target_it->second->outer_url());
  EXPECT_EQ(target_url, target_it->second->inner_url());
  EXPECT_EQ(target_header_integrity, *target_it->second->header_integrity());

  const auto image1_it = cached_exchanges.find(image1_sxg_url);
  ASSERT_TRUE(image1_it != cached_exchanges.end());
  EXPECT_EQ(image1_sxg_url, image1_it->second->outer_url());
  EXPECT_EQ(image1_url, image1_it->second->inner_url());
  EXPECT_EQ(image1_header_integrity, *image1_it->second->header_integrity());

  NavigateToURLAndWaitTitle(target_sxg_url, "done");

  EXPECT_EQ(1, image1_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, image1_request_counter->GetRequestCount());
  EXPECT_EQ(0, image2_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, image2_request_counter->GetRequestCount());
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       MultipleResources) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* script1_sxg_path = "/script1_js.sxg";
  const char* script1_path = "/script1.js";
  const char* script2_sxg_path = "/script2_js.sxg";
  const char* script2_path = "/script2.js";

  auto script1_sxg_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), script1_sxg_path);
  auto script2_sxg_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), script2_sxg_path);
  auto script1_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), script1_path);
  auto script2_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), script2_path);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  const GURL script1_sxg_url = embedded_test_server()->GetURL(script1_sxg_path);
  const GURL script1_url = embedded_test_server()->GetURL(script1_path);
  const GURL script2_sxg_url = embedded_test_server()->GetURL(script2_sxg_path);
  const GURL script2_url = embedded_test_server()->GetURL(script2_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue script1_header_integrity = {{0x02}};
  const net::SHA256HashValue script2_header_integrity = {{0x03}};

  const std::string outer_link_header = base::JoinString(
      {CreateAlternateLinkHeader(script1_sxg_url, script1_url),
       CreateAlternateLinkHeader(script2_sxg_url, script2_url)},
      ",");
  const std::string inner_link_headers =
      base::JoinString(
          {CreateAllowedAltSxgLinkHeader(script1_url, script1_header_integrity),
           CreateAllowedAltSxgLinkHeader(script2_url, script2_header_integrity),
           CreatePreloadLinkHeader(script1_url, "script"),
           CreatePreloadLinkHeader(script2_url, "script")},
          ",");
  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(script1_path, ResponseEntry("var test_title=\"from\";",
                                               "text/javascript"));
  RegisterResponse(script2_path,
                   ResponseEntry("document.title=test_title+\"server\";",
                                 "text/javascript"));
  RegisterResponse(target_sxg_path,
                   CreateSignedExchangeResponseEntry(
                       "<head><title>Prefetch Target (SXG)</title>"
                       "<script src=\"./script1.js\"></script>"
                       "<script src=\"./script2.js\"></script></head>",
                       {{"link", outer_link_header}}));
  RegisterResponse(script1_sxg_path, CreateSignedExchangeResponseEntry(
                                         "var test_title=\"done\";"));
  RegisterResponse(script2_sxg_path, CreateSignedExchangeResponseEntry(
                                         "document.title=test_title;"));
  MockSignedExchangeHandlerFactory factory({
      MockSignedExchangeHandlerParams(
          target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
          target_url, "text/html", {{"Link", inner_link_headers}},
          target_header_integrity),
      MockSignedExchangeHandlerParams(
          script1_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
          script1_url, "text/javascript", {}, script1_header_integrity),
      MockSignedExchangeHandlerParams(
          script2_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
          script2_url, "text/javascript", {}, script2_header_integrity),
  });
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(script1_sxg_url);
  WaitUntilLoaded(script2_sxg_url);

  EXPECT_EQ(1, script1_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, script1_request_counter->GetRequestCount());
  EXPECT_EQ(1, script2_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, script2_request_counter->GetRequestCount());

  const auto cached_exchanges = GetCachedExchanges(shell());
  EXPECT_EQ(3u, cached_exchanges.size());

  const auto target_it = cached_exchanges.find(target_sxg_url);
  ASSERT_TRUE(target_it != cached_exchanges.end());
  EXPECT_EQ(target_sxg_url, target_it->second->outer_url());
  EXPECT_EQ(target_url, target_it->second->inner_url());
  EXPECT_EQ(target_header_integrity, *target_it->second->header_integrity());

  const auto script1_it = cached_exchanges.find(script1_sxg_url);
  ASSERT_TRUE(script1_it != cached_exchanges.end());
  EXPECT_EQ(script1_sxg_url, script1_it->second->outer_url());
  EXPECT_EQ(script1_url, script1_it->second->inner_url());
  EXPECT_EQ(script1_header_integrity, *script1_it->second->header_integrity());

  const auto script2_it = cached_exchanges.find(script2_sxg_url);
  ASSERT_TRUE(script2_it != cached_exchanges.end());
  EXPECT_EQ(script2_sxg_url, script2_it->second->outer_url());
  EXPECT_EQ(script2_url, script2_it->second->inner_url());
  EXPECT_EQ(script2_header_integrity, *script2_it->second->header_integrity());

  // Subsequent navigation to the target URL wouldn't hit the network for
  // the target URL. The target content should still be read correctly.
  // The content is loaded from PrefetchedSignedExchangeCache. And the scripts
  // are also loaded from PrefetchedSignedExchangeCache.
  NavigateToURLAndWaitTitle(target_sxg_url, "done");

  EXPECT_EQ(1, script1_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, script1_request_counter->GetRequestCount());
  EXPECT_EQ(1, script2_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, script2_request_counter->GetRequestCount());
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       IntegrityMismatch) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* script_path = "/script.js";
  const char* script_sxg_path = "/script_js.sxg";

  auto script_sxg_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), script_sxg_path);
  auto script_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), script_path);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  const GURL script_sxg_url = embedded_test_server()->GetURL(script_sxg_path);
  const GURL script_url = embedded_test_server()->GetURL(script_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue script_header_integrity = {{0x02}};
  const net::SHA256HashValue wrong_script_header_integrity = {{0x03}};

  const std::string outer_link_header =
      CreateAlternateLinkHeader(script_sxg_url, script_url);
  // Use the wrong header integrity value for "allowed-alt-sxg" link header to
  // trigger the integrity mismatch fallback logic.
  const std::string inner_link_headers =
      base::JoinString({CreateAllowedAltSxgLinkHeader(
                            script_url, wrong_script_header_integrity),
                        CreatePreloadLinkHeader(script_url, "script")},
                       ",");

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(script_path, ResponseEntry("document.title=\"from server\";",
                                              "text/javascript"));
  RegisterResponse(target_sxg_path,
                   CreateSignedExchangeResponseEntry(
                       "<head><title>Prefetch Target (SXG)</title>"
                       "<script src=\"./script.js\"></script></head>",
                       {{"link", outer_link_header}}));
  RegisterResponse(script_sxg_path, CreateSignedExchangeResponseEntry(
                                        "document.title=\"done\";"));
  MockSignedExchangeHandlerFactory factory(
      {MockSignedExchangeHandlerParams(
           target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           target_url, "text/html", {{"Link", inner_link_headers}},
           target_header_integrity),
       MockSignedExchangeHandlerParams(
           script_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           script_url, "text/javascript", {}, script_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(script_sxg_url);

  EXPECT_EQ(1, script_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, script_request_counter->GetRequestCount());

  // The value of "header-integrity" in "allowed-alt-sxg" link header of the
  // inner response doesn't match the actual header integrity of script_js.sxg.
  // So the script request must go to the server.
  NavigateToURLAndWaitTitle(target_sxg_url, "from server");

  EXPECT_EQ(1, script_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(1, script_request_counter->GetRequestCount());
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       IntegrityMismatchSendsReport) {
  net::test_server::EmbeddedTestServer ssl_server(
      net::test_server::EmbeddedTestServer::TYPE_HTTPS);

  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* script_path = "/script.js";
  const char* script_sxg_path = "/script_js.sxg";
  const char* learn_report_to_path = "/report-to";
  const char* report_path = "/report";

  net::test_server::ControllableHttpResponse learn_report_to_response(
      &ssl_server, learn_report_to_path);
  net::test_server::ControllableHttpResponse report_response(&ssl_server,
                                                             report_path);

  RegisterRequestHandler(&ssl_server);
  ASSERT_TRUE(ssl_server.Start());

  const GURL report_url = ssl_server.GetURL(report_path);

  // Get Report-To and NEL information for the server that serves the signed
  // exchange. The same site is also used for the NetworkAnonymizationKey. This
  // should result in sending a report to that server a request for that signed
  // exchange fails with a certificate error.
  {
    const GURL learn_report_to_url = ssl_server.GetURL(learn_report_to_path);
    TestNavigationObserver same_tab_observer(shell()->web_contents(),
                                             1 /* number_of_navigations */);
    NavigationController::LoadURLParams params(learn_report_to_url);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
    shell()->web_contents()->GetController().LoadURLWithParams(params);
    learn_report_to_response.WaitForRequest();
    learn_report_to_response.Send("HTTP/1.1 200 OK\r\n");
    learn_report_to_response.Send("Report-To: {\"endpoints\":[{\"url\":\"" +
                                  report_url.spec() +
                                  "\"}],\"max_age\":86400}\r\n");
    learn_report_to_response.Send(
        "NEL: {\"report_to\":\"default\",\"max_age\":86400}\r\n");
    learn_report_to_response.Send("\r\n");
    learn_report_to_response.Done();
    same_tab_observer.Wait();
  }

  const GURL target_sxg_url = ssl_server.GetURL(target_sxg_path);
  const GURL target_url = ssl_server.GetURL(target_path);
  const GURL script_sxg_url = ssl_server.GetURL(script_sxg_path);
  const GURL script_url = ssl_server.GetURL(script_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue script_header_integrity = {{0x02}};
  const net::SHA256HashValue wrong_script_header_integrity = {{0x03}};

  const std::string outer_link_header =
      CreateAlternateLinkHeader(script_sxg_url, script_url);
  // Use the wrong header integrity value for "allowed-alt-sxg" link header to
  // trigger the integrity mismatch fallback logic.
  const std::string inner_link_headers = base::JoinString(
      {CreateAllowedAltSxgLinkHeader(script_url, wrong_script_header_integrity),
       CreatePreloadLinkHeader(script_url, "script")},
      ",");

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(script_path, ResponseEntry("document.title=\"from server\";",
                                              "text/javascript"));
  RegisterResponse(target_sxg_path,
                   CreateSignedExchangeResponseEntry(
                       "<head><title>Prefetch Target (SXG)</title>"
                       "<script src=\"./script.js\"></script></head>",
                       {{"link", outer_link_header}}));
  RegisterResponse(script_sxg_path, CreateSignedExchangeResponseEntry(
                                        "document.title=\"done\";"));
  MockSignedExchangeHandlerFactory factory(
      {MockSignedExchangeHandlerParams(
           target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           target_url, "text/html", {{"Link", inner_link_headers}},
           target_header_integrity),
       MockSignedExchangeHandlerParams(
           script_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           script_url, "text/javascript", {}, script_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  EXPECT_TRUE(NavigateToURL(shell(), ssl_server.GetURL(prefetch_path)));

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(script_sxg_url);

  // The value of "header-integrity" in "allowed-alt-sxg" link header of the
  // inner response doesn't match the actual header integrity of script_js.sxg.
  // So the script request must go to the server.
  NavigateToURLAndWaitTitle(target_sxg_url, "from server");

  // Check that a report was received.
  report_response.WaitForRequest();
  EXPECT_EQ(report_url, report_response.http_request()->GetURL());
  EXPECT_EQ(net::test_server::METHOD_POST,
            report_response.http_request()->method);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       MultipleResources_IntegrityMismatch) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* script1_sxg_path = "/script1_js.sxg";
  const char* script1_path = "/script1.js";
  const char* script2_sxg_path = "/script2_js.sxg";
  const char* script2_path = "/script2.js";

  auto script1_sxg_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), script1_sxg_path);
  auto script2_sxg_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), script2_sxg_path);
  auto script1_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), script1_path);
  auto script2_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), script2_path);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  const GURL script1_sxg_url = embedded_test_server()->GetURL(script1_sxg_path);
  const GURL script1_url = embedded_test_server()->GetURL(script1_path);
  const GURL script2_sxg_url = embedded_test_server()->GetURL(script2_sxg_path);
  const GURL script2_url = embedded_test_server()->GetURL(script2_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue script1_header_integrity = {{0x02}};
  const net::SHA256HashValue script2_header_integrity = {{0x03}};
  const net::SHA256HashValue wrong_script2_header_integrity = {{0x04}};

  const std::string outer_link_header = base::JoinString(
      {CreateAlternateLinkHeader(script1_sxg_url, script1_url),
       CreateAlternateLinkHeader(script2_sxg_url, script2_url)},
      ",");
  // Use the wrong header integrity value for "allowed-alt-sxg" link header for
  // script2 to trigger the integrity mismatch fallback logic.
  const std::string inner_link_headers =
      base::JoinString(
          {CreateAllowedAltSxgLinkHeader(script1_url, script1_header_integrity),
           CreateAllowedAltSxgLinkHeader(script2_url,
                                         wrong_script2_header_integrity),
           CreatePreloadLinkHeader(script1_url, "script"),
           CreatePreloadLinkHeader(script2_url, "script")},
          ",");

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(script1_path, ResponseEntry("var test_title=\"from\";",
                                               "text/javascript"));
  RegisterResponse(script2_path,
                   ResponseEntry("document.title=test_title+\" server\";",
                                 "text/javascript"));
  RegisterResponse(target_sxg_path,
                   CreateSignedExchangeResponseEntry(
                       "<head><title>Prefetch Target (SXG)</title>"
                       "<script src=\"./script1.js\"></script>"
                       "<script src=\"./script2.js\"></script></head>",
                       {{"link", outer_link_header}}));
  RegisterResponse(script1_sxg_path, CreateSignedExchangeResponseEntry(
                                         "var test_title=\"done\";"));
  RegisterResponse(script2_sxg_path, CreateSignedExchangeResponseEntry(
                                         "document.title=test_title;"));
  MockSignedExchangeHandlerFactory factory({
      MockSignedExchangeHandlerParams(
          target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
          target_url, "text/html", {{"Link", inner_link_headers}},
          target_header_integrity),
      MockSignedExchangeHandlerParams(
          script1_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
          script1_url, "text/javascript", {}, script1_header_integrity),
      MockSignedExchangeHandlerParams(
          script2_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
          script2_url, "text/javascript", {}, script2_header_integrity),
  });
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(script1_sxg_url);
  WaitUntilLoaded(script2_sxg_url);

  EXPECT_EQ(1, script1_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, script1_request_counter->GetRequestCount());
  EXPECT_EQ(1, script2_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, script2_request_counter->GetRequestCount());

  const auto cached_exchanges = GetCachedExchanges(shell());
  EXPECT_EQ(3u, cached_exchanges.size());

  // The value of "header-integrity" in "allowed-alt-sxg" link header of the
  // inner response doesn't match the actual header integrity of script2_js.sxg.
  // So the all script requests must go to the server.
  NavigateToURLAndWaitTitle(target_sxg_url, "from server");

  EXPECT_EQ(1, script1_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(1, script1_request_counter->GetRequestCount());
  EXPECT_EQ(1, script2_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(1, script2_request_counter->GetRequestCount());
}

// TODO(crbug.com/1350046): Flaky.
IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       DISABLED_CORS) {
  std::unique_ptr<net::EmbeddedTestServer> data_server =
      std::make_unique<net::EmbeddedTestServer>(
          net::EmbeddedTestServer::TYPE_HTTPS);

  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";

  RegisterRequestHandler(embedded_test_server());
  RegisterRequestHandler(data_server.get());

  // Prefetch requests for alternate SXG should be made with cors, regardless of
  // the crossorigin attribute of Link:rel=preload header that triggered the
  // prefetch.
  embedded_test_server()->RegisterRequestMonitor(
      base::BindRepeating([](const net::test_server::HttpRequest& request) {
        if (!base::EndsWith(request.relative_url, "_data.sxg"))
          return;
        auto it = request.headers.find("Sec-Fetch-Mode");
        ASSERT_TRUE(it != request.headers.end());
        EXPECT_EQ(it->second, "cors");
      }));

  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(data_server->Start());

  std::string test_server_origin = embedded_test_server()->base_url().spec();
  // Remove the last "/""
  test_server_origin.pop_back();

  struct {
    // Set in the main SXG's inner response header.
    // Example:
    //   Link: <http://***/**.data>;rel="preload";as="fetch";crossorigin
    //                                                       ^^^^^^^^^^^
    const char* const crossorigin_preload_header;
    // Set in the data SXG's inner response header.
    // Example:
    //   Access-Control-Allow-Origin: *
    //                                ^
    const char* const access_control_allow_origin_header;
    // Set "Access-Control-Allow-Credentials: true" link header in the data
    // SXG's inner response header.
    const bool has_access_control_allow_credentials_true_header;
    // The credentials attribute of Fetch API's request while fetching the data.
    const char* const request_credentials;
    // If the data is served from the SXG the result must be "sxg". If the data
    // is served from the server without SXG, the result must be "server". If
    // failed to fetch the data, the result must be "failed".
    const char* const expected_result;
  } kTestCases[] = {
      // - If crossorigin is not set in the preload header, cross origin fetch
      //   goes to the server. It is because the mode of the preload request
      //   ("no-cors") and the mode of the fetch request ("cors") doesn't match.
      {nullptr, nullptr, false, "omit", "server"},
      {nullptr, nullptr, false, "include", "server"},
      {nullptr, nullptr, false, "same-origin", "server"},

      // - When "crossorigin" is set in the preload header, the mode of the
      //   preload request is "cors", and the credentials mode is "same-origin".
      //   - When the credentials mode of the fetch request doesn't match, the
      //     fetch request goes to the server.
      {"crossorigin", nullptr, false, "omit", "server"},
      {"crossorigin", nullptr, false, "include", "server"},
      //   - When the credentials mode of the fetch request match with the
      //     credentials mode of the preload request, the prefetched signed
      //     exchange is used.
      //     - When the inner response doesn't have ACAO header, fails to load.
      {"crossorigin", nullptr, false, "same-origin", "failed"},
      //     - When the inner response has "ACAO: *" header, succeeds to load.
      {"crossorigin", "*", false, "same-origin", "sxg"},
      //     - When the inner response has "ACAO: <origin>" header, succeeds to
      //       load.
      {"crossorigin", test_server_origin.c_str(), false, "same-origin", "sxg"},

      // - crossorigin="anonymous" must be treated as same as just having
      //   "crossorigin".
      {"crossorigin=\"anonymous\"", nullptr, false, "omit", "server"},
      {"crossorigin=\"anonymous\"", nullptr, false, "include", "server"},
      {"crossorigin=\"anonymous\"", nullptr, false, "same-origin", "failed"},
      {"crossorigin=\"anonymous\"", "*", false, "same-origin", "sxg"},
      {"crossorigin=\"anonymous\"", test_server_origin.c_str(), false,
       "same-origin", "sxg"},

      // - When crossorigin="use-credentials" is set in the preload header, the
      //   mode of the preload request is "cors", and the credentials mode is
      //   "include".
      //  - When the credentials mode of the fetch request doesn't match, the
      //    fetch request goes to the server.
      {"crossorigin=\"use-credentials\"", nullptr, false, "omit", "server"},
      {"crossorigin=\"use-credentials\"", nullptr, false, "same-origin",
       "server"},
      //   - When the credentials mode of the fetch request match with the
      //     credentials mode of the preload request, the prefetched signed
      //     exchange is used.
      //     - When the inner response doesn't have ACAO header, fails to load.
      {"crossorigin=\"use-credentials\"", nullptr, false, "include", "failed"},
      //     - Even if the inner response has "ACAO: *" header, fails to load
      //       the "include" credentials mode request.
      {"crossorigin=\"use-credentials\"", "*", false, "include", "failed"},
      //     - Even if the inner response has "ACAO: *" header and "ACAC: true"
      //       header, fails to load the "include" credentials mode request.
      {"crossorigin=\"use-credentials\"", "*", true, "include", "failed"},
      //     - Even if the inner response has "ACAO: <origin>" header, fails to
      //       load the "include" credentials mode request.
      {"crossorigin=\"use-credentials\"", test_server_origin.c_str(), false,
       "include", "failed"},
      //     - If the inner response has "ACAO: <origin>" header, and
      //       "ACAC: true" header, succeeds to load.
      {"crossorigin=\"use-credentials\"", test_server_origin.c_str(), true,
       "include", "sxg"},
  };

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};

  std::string target_sxg_outer_link_header;
  std::string target_sxg_inner_link_header;
  std::string requests_list_string;

  std::vector<MockSignedExchangeHandlerParams> mock_params;
  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    if (i) {
      target_sxg_outer_link_header += ",";
      target_sxg_inner_link_header += ",";
      requests_list_string += ",";
    }
    const std::string data_sxg_path = base::StringPrintf("/%zu_data.sxg", i);
    const std::string data_path = base::StringPrintf("/%zu.data", i);
    const GURL data_sxg_url = embedded_test_server()->GetURL(data_sxg_path);
    const GURL data_url = data_server->GetURL(data_path);
    requests_list_string += base::StringPrintf(
        "new Request('%s', {credentials: '%s'})", data_url.spec().c_str(),
        kTestCases[i].request_credentials);
    const net::SHA256HashValue data_header_integrity = {
        {static_cast<uint8_t>(0x02 + i)}};

    target_sxg_outer_link_header +=
        CreateAlternateLinkHeader(data_sxg_url, data_url);
    target_sxg_inner_link_header += base::JoinString(
        {CreateAllowedAltSxgLinkHeader(data_url, data_header_integrity),
         CreatePreloadLinkHeader(data_url, "fetch")},
        ",");
    if (kTestCases[i].crossorigin_preload_header) {
      target_sxg_inner_link_header +=
          base::StringPrintf(";%s", kTestCases[i].crossorigin_preload_header);
    }
    RegisterResponse(data_sxg_path, CreateSignedExchangeResponseEntry("sxg"));
    RegisterResponse(
        data_path,
        ResponseEntry(
            "server", "text/plain",
            {{"Access-Control-Allow-Origin", test_server_origin.c_str()},
             {"Access-Control-Allow-Credentials", "true"}}));
    std::vector<std::pair<std::string, std::string>> data_sxg_inner_headers;
    if (kTestCases[i].access_control_allow_origin_header) {
      data_sxg_inner_headers.emplace_back(std::make_pair(
          std::string("Access-Control-Allow-Origin"),
          std::string(kTestCases[i].access_control_allow_origin_header)));
    }
    if (kTestCases[i].has_access_control_allow_credentials_true_header) {
      data_sxg_inner_headers.emplace_back(
          std::make_pair(std::string("Access-Control-Allow-Credentials"),
                         std::string("true")));
    }
    mock_params.emplace_back(
        data_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK, data_url,
        "text/plain", std::move(data_sxg_inner_headers), data_header_integrity);
  }

  std::vector<std::pair<std::string, std::string>> target_sxg_inner_headers = {
      {"Link", target_sxg_inner_link_header}};
  mock_params.emplace_back(target_sxg_url, SignedExchangeLoadResult::kSuccess,
                           net::OK, target_url, "text/html",
                           target_sxg_inner_headers, target_header_integrity);
  MockSignedExchangeHandlerFactory factory(std::move(mock_params));

  RegisterResponse(
      prefetch_path,
      ResponseEntry(base::StringPrintf(
          "<body><link rel='prefetch' href='%s'></body>", target_sxg_path)));
  RegisterResponse(target_sxg_path,
                   CreateSignedExchangeResponseEntry(
                       base::StringPrintf(R"(
<head><title>Prefetch Target (SXG)</title><script>
  const requests = [%s];
  const promises = [];
  for (const request of requests) {
    promises.push((async (request) => {
      try {
        const res = await fetch(request);
        const result = await res.text();
        console.log(request.url + ': ' + result);
        return result;
      } catch (err) {
        console.log(request.url + ': failed');
        return 'failed';
      }
    })(request));
  }
  let results = null;
  Promise.all(promises).then((values) => {
    results = values;
    document.title = 'done';
  });
</script></head>)",
                                          requests_list_string.c_str()),
                       {{"link", target_sxg_outer_link_header}}));

  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  // Wait until all (main- and sub-resource) SXGs are prefetched.
  while (GetCachedExchanges(shell()).size() < std::size(kTestCases) + 1) {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  NavigateToURLAndWaitTitle(target_sxg_url, "done");

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(base::StringPrintf("TestCase %zu", i));
    EXPECT_EQ(
        EvalJs(shell(), base::StringPrintf("results[%zu]", i)).ExtractString(),
        kTestCases[i].expected_result);
  }
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       CrossOriginReadBlocking_AllowedAfterSniffing) {
  RunCrossOriginReadBlockingTest(
      true /* cross_origin */, "document.title=\"done\"", "text/javascript",
      false /* has_nosniff */, true /* corb_allowed */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       CrossOriginReadBlocking_BlockedAfterSniffing) {
  RunCrossOriginReadBlockingTest(
      true /* cross_origin */, "<!DOCTYPE html>hello;", "text/html",
      false /* has_nosniff */, false /* corb_allowed */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       CrossOriginReadBlocking_AllowedWithoutSniffing) {
  RunCrossOriginReadBlockingTest(
      false /* cross_origin */, "document.title=\"done\"", "text/javascript",
      true /* has_nosniff */, true /* corb_allowed */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       CrossOriginReadBlocking_BlockedWithoutSniffing) {
  RunCrossOriginReadBlockingTest(
      true /* cross_origin */, "<!DOCTYPE html>hello;", "text/html",
      true /* has_nosniff */, false /* corb_allowed */);
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       ScriptSXGNotGCed) {
  const char* prefetch_page_path = "/prefetch.html";
  const char* page_sxg_path = "/target.sxg";
  const char* page_inner_url_path = "/target.html";
  const char* script_sxg_path = "/script.sxg";
  const char* script_inner_url_path = "/script.js";

  auto script_sxg_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), script_sxg_path);
  auto script_request_counter = RequestCounter::CreateAndMonitor(
      embedded_test_server(), script_inner_url_path);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL prefetch_page_url =
      embedded_test_server()->GetURL(prefetch_page_path);
  const GURL sxg_page_url = embedded_test_server()->GetURL(page_sxg_path);
  const GURL inner_url_page_url =
      embedded_test_server()->GetURL(page_inner_url_path);
  const GURL sxg_script_url = embedded_test_server()->GetURL(script_sxg_path);
  const GURL inner_url_script_url =
      embedded_test_server()->GetURL(script_inner_url_path);

  const net::SHA256HashValue page_header_integrity = {{0x01}};
  const net::SHA256HashValue script_header_integrity = {{0x02}};

  const std::string outer_link_header =
      CreateAlternateLinkHeader(sxg_script_url, inner_url_script_url);
  const std::string inner_link_headers =
      base::JoinString(
          {CreateAllowedAltSxgLinkHeader(inner_url_script_url,
                                         script_header_integrity),
           CreatePreloadLinkHeader(inner_url_script_url, "script")},
          ",");

  RegisterResponse(prefetch_page_path,
                   ResponseEntry(base::StringPrintf(
                       "<body><link rel='prefetch' href='%s'></body>",
                       sxg_page_url.spec().c_str())));
  RegisterResponse(
      page_sxg_path,
      CreateSignedExchangeResponseEntry(
          "<head><title>Prefetch Target (SXG)</title>"
          "<script src=\"./script.js\" async defer></script></head>",
          {{"link", outer_link_header}}));
  RegisterResponse(script_sxg_path, CreateSignedExchangeResponseEntry(
                                        "document.title=\"done\";", {}));
  MockSignedExchangeHandlerFactory factory(
      {MockSignedExchangeHandlerParams(
           sxg_page_url, SignedExchangeLoadResult::kSuccess, net::OK,
           inner_url_page_url, "text/html", {{"Link", inner_link_headers}},
           page_header_integrity),
       MockSignedExchangeHandlerParams(
           sxg_script_url, SignedExchangeLoadResult::kSuccess, net::OK,
           inner_url_script_url, "text/javascript",
           // Set "cache-control: public" to keep the script in the memory
           // cache.
           {{"cache-control", "public, max-age=600"}},
           script_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());
  EXPECT_TRUE(NavigateToURL(shell(), prefetch_page_url));

  WaitUntilLoaded(sxg_page_url);
  WaitUntilLoaded(sxg_script_url);

  EXPECT_EQ(1, script_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, script_request_counter->GetRequestCount());
  EXPECT_EQ(2, GetPrefetchURLLoaderCallCount());

  const auto cached_exchanges = GetCachedExchanges(shell());
  EXPECT_EQ(2u, cached_exchanges.size());

  NavigateToURLAndWaitTitle(sxg_page_url, "done");

  EXPECT_EQ(1, script_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, script_request_counter->GetRequestCount());

  // Clears the title.
  EXPECT_TRUE(ExecJs(shell()->web_contents(), "document.title = '';"));

  const char* next_page_path = "/next_page.html";
  const GURL next_page_url = embedded_test_server()->GetURL(next_page_path);
  RegisterResponse(
      next_page_path,
      ResponseEntry(
          "<head><title>Next page</title>"
          "<script src=\"./script.js\" async defer></script></head>"));
  // Triggers GC.
  EXPECT_TRUE(ExecJs(shell()->web_contents(), "window.gc();"));
  // The script which was served via SXG must be kept in memory cache and must
  // be reused.
  NavigateToURLAndWaitTitle(next_page_url, "done");

  EXPECT_EQ(0, script_request_counter->GetRequestCount());
}

IN_PROC_BROWSER_TEST_F(SignedExchangeSubresourcePrefetchBrowserTest,
                       DoNotSendUnrelatedSXG) {
  const char* prefetch_path = "/prefetch.html";
  const char* target_sxg_path = "/target.sxg";
  const char* target_path = "/target.html";
  const char* script_path = "/script.js";
  const char* script_sxg_path = "/script_js.sxg";

  auto script_sxg_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), script_sxg_path);
  auto script_request_counter =
      RequestCounter::CreateAndMonitor(embedded_test_server(), script_path);
  RegisterRequestHandler(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_EQ(0, GetPrefetchURLLoaderCallCount());

  const GURL target_sxg_url = embedded_test_server()->GetURL(target_sxg_path);
  const GURL target_url = embedded_test_server()->GetURL(target_path);
  const GURL script_sxg_url = embedded_test_server()->GetURL(script_sxg_path);
  const GURL script_url = embedded_test_server()->GetURL(script_path);

  const net::SHA256HashValue target_header_integrity = {{0x01}};
  const net::SHA256HashValue script_header_integrity = {{0x02}};

  RegisterResponse(prefetch_path, ResponseEntry(base::StringPrintf(
                                      "<body><link rel='prefetch' href='%s'>"
                                      "<link rel='prefetch' href='%s'></body>",
                                      target_sxg_path, script_sxg_path)));
  RegisterResponse(script_path, ResponseEntry("document.title=\"from server\";",
                                              "text/javascript"));
  RegisterResponse(target_sxg_path,
                   CreateSignedExchangeResponseEntry(
                       "<head><title>Prefetch Target (SXG)</title>"
                       "<script src=\"./script.js\"></script></head>"));
  RegisterResponse(script_sxg_path, CreateSignedExchangeResponseEntry(
                                        "document.title=\"from sxg\";"));
  MockSignedExchangeHandlerFactory factory(
      {MockSignedExchangeHandlerParams(
           target_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           target_url, "text/html", {}, target_header_integrity),
       MockSignedExchangeHandlerParams(
           script_sxg_url, SignedExchangeLoadResult::kSuccess, net::OK,
           script_url, "text/javascript", {}, script_header_integrity)});
  ScopedSignedExchangeHandlerFactory scoped_factory(&factory);

  EXPECT_TRUE(
      NavigateToURL(shell(), embedded_test_server()->GetURL(prefetch_path)));

  WaitUntilLoaded(target_sxg_url);
  WaitUntilLoaded(script_sxg_url);

  EXPECT_EQ(1, script_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(0, script_request_counter->GetRequestCount());

  NavigationHandleSXGAttributeObserver observer(shell()->web_contents());
  NavigateToURLAndWaitTitle(target_sxg_url, "from server");

  EXPECT_EQ(1, script_sxg_request_counter->GetRequestCount());
  EXPECT_EQ(1, script_request_counter->GetRequestCount());
  ASSERT_TRUE(observer.had_prefetched_alt_sxg().has_value());
  EXPECT_FALSE(observer.had_prefetched_alt_sxg().value());
}

}  // namespace content

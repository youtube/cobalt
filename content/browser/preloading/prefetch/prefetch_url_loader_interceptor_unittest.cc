// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_url_loader_interceptor.h"

#include <map>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/elapsed_timer.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_features.h"
#include "content/browser/preloading/prefetch/prefetch_match_resolver.h"
#include "content/browser/preloading/prefetch/prefetch_origin_prober.h"
#include "content/browser/preloading/prefetch/prefetch_params.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_service.h"
#include "content/browser/preloading/prefetch/prefetch_test_utils.h"
#include "content/browser/preloading/prefetch/prefetch_type.h"
#include "content/browser/preloading/prefetch/prefetch_url_loader_helper.h"
#include "content/browser/preloading/preloading.h"
#include "content/browser/preloading/preloading_data_impl.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/preloading_test_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_content_browser_client.h"
#include "net/base/isolation_info.h"
#include "net/base/load_timing_info.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

namespace content {
namespace {

// These tests leak mojo objects (like the PrefetchFromStringURLLoader) because
// they do not have valid mojo channels, which would normally delete the bound
// objects on destruction. This is expected and cannot be easily fixed without
// rewriting these as browsertests. The trade off for the speed and flexibility
// of unittests is an intentional decision.
#if defined(LEAK_SANITIZER)
#define DISABLE_ASAN(x) DISABLED_##x
#else
#define DISABLE_ASAN(x) x
#endif

const char kDNSCanaryCheckAddress[] = "http://testdnscanarycheck.com";
const char kTLSCanaryCheckAddress[] = "http://testtlscanarycheck.com";

PreloadingFailureReason ToPreloadingFailureReason(PrefetchStatus status) {
  return static_cast<PreloadingFailureReason>(
      static_cast<int>(status) +
      static_cast<int>(
          PreloadingFailureReason::kPreloadingFailureReasonCommonEnd));
}

class TestPrefetchOriginProber : public PrefetchOriginProber {
 public:
  TestPrefetchOriginProber(BrowserContext* browser_context,
                           bool should_probe_origins_response,
                           const GURL& probe_url,
                           PrefetchProbeResult probe_result)
      : PrefetchOriginProber(browser_context,
                             GURL(kDNSCanaryCheckAddress),
                             GURL(kTLSCanaryCheckAddress)),
        should_probe_origins_response_(should_probe_origins_response),
        probe_url_(probe_url),
        probe_result_(probe_result) {}

  bool ShouldProbeOrigins() const override {
    return should_probe_origins_response_;
  }

  void Probe(const GURL& url, OnProbeResultCallback callback) override {
    EXPECT_TRUE(should_probe_origins_response_);
    EXPECT_EQ(url, probe_url_);

    num_probes_++;

    std::move(callback).Run(probe_result_);
  }

  int num_probes() const { return num_probes_; }

 private:
  bool should_probe_origins_response_;

  GURL probe_url_;
  PrefetchProbeResult probe_result_;

  int num_probes_{0};
};

class ScopedMockContentBrowserClient : public TestContentBrowserClient {
 public:
  ScopedMockContentBrowserClient() {
    old_browser_client_ = SetBrowserClientForTesting(this);
  }

  ~ScopedMockContentBrowserClient() override {
    EXPECT_EQ(this, SetBrowserClientForTesting(old_browser_client_));
  }

  MOCK_METHOD(
      bool,
      WillCreateURLLoaderFactory,
      (BrowserContext * browser_context,
       RenderFrameHost* frame,
       int render_process_id,
       URLLoaderFactoryType type,
       const url::Origin& request_initiator,
       absl::optional<int64_t> navigation_id,
       ukm::SourceIdObj ukm_source_id,
       mojo::PendingReceiver<network::mojom::URLLoaderFactory>*
           factory_receiver,
       mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
           header_client,
       bool* bypass_redirect_checks,
       bool* disable_secure_dns,
       network::mojom::URLLoaderFactoryOverridePtr* factory_override,
       scoped_refptr<base::SequencedTaskRunner>
           navigation_response_task_runner),
      (override));

 private:
  raw_ptr<ContentBrowserClient> old_browser_client_;
};

class TestPrefetchService : public PrefetchService {
 public:
  explicit TestPrefetchService(BrowserContext* browser_context)
      : PrefetchService(browser_context) {}

  void TakePrefetchOriginProber(
      std::unique_ptr<TestPrefetchOriginProber> test_origin_prober) {
    test_origin_prober_ = std::move(test_origin_prober);
  }

  void AddOnStartCookieCopyClosure(const GURL& prefetch_url,
                                   const GURL& redirect_url,
                                   base::OnceClosure closure) {
    auto key = std::make_pair(prefetch_url, redirect_url);
    EXPECT_TRUE(on_start_cookie_copy_closure_.find(key) ==
                on_start_cookie_copy_closure_.end());

    on_start_cookie_copy_closure_[key] = std::move(closure);
  }

  int num_probes() const { return test_origin_prober_->num_probes(); }

 private:
  PrefetchOriginProber* GetPrefetchOriginProber() const override {
    return test_origin_prober_.get();
  }

  void CopyIsolatedCookies(const PrefetchContainer::Reader& reader) override {
    if (!reader.IsIsolatedNetworkContextRequiredToServe()) {
      return;
    }

    reader.OnIsolatedCookieCopyStart();

    auto itr = on_start_cookie_copy_closure_.find(
        std::make_pair(reader.GetPrefetchContainer()->GetURL(),
                       reader.GetCurrentURLToServe()));
    EXPECT_TRUE(itr != on_start_cookie_copy_closure_.end());
    EXPECT_TRUE(itr->second);
    std::move(itr->second).Run();
  }

  std::unique_ptr<TestPrefetchOriginProber> test_origin_prober_;

  std::map<std::pair<GURL, GURL>, base::OnceClosure>
      on_start_cookie_copy_closure_;
};

class TestPrefetchURLLoaderInterceptor : public PrefetchURLLoaderInterceptor {
 public:
  TestPrefetchURLLoaderInterceptor(
      int frame_tree_node_id,
      const blink::DocumentToken& initiator_document_token)
      : PrefetchURLLoaderInterceptor(frame_tree_node_id,
                                     initiator_document_token) {}
  ~TestPrefetchURLLoaderInterceptor() override = default;

  void AddPrefetch(base::WeakPtr<PrefetchContainer> prefetch_container) {
    prefetches_[prefetch_container->GetURL()] = prefetch_container;
  }

 private:
  void GetPrefetch(const network::ResourceRequest& tentative_resource_request,
                   PrefetchMatchResolver& prefetch_match_resolver,
                   base::OnceCallback<void(PrefetchContainer::Reader)>
                       get_prefetch_callback) const override {
    const auto& iter = prefetches_.find(tentative_resource_request.url);
    if (iter == prefetches_.end()) {
      std::move(get_prefetch_callback).Run({});
      return;
    }

    OnGotPrefetchToServe(GetFrameTreeNodeId(), tentative_resource_request,
                         std::move(get_prefetch_callback),
                         iter->second ? iter->second->CreateReader()
                                      : PrefetchContainer::Reader());
  }

  std::map<GURL, base::WeakPtr<PrefetchContainer>> prefetches_;
};

class PrefetchURLLoaderInterceptorTestBase : public RenderViewHostTestHarness {
 public:
  PrefetchURLLoaderInterceptorTestBase()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    test_content_browser_client_ =
        std::make_unique<ScopedMockContentBrowserClient>();

    std::unique_ptr<TestPrefetchService> prefetch_service =
        std::make_unique<TestPrefetchService>(browser_context());

    PrefetchService::SetFromFrameTreeNodeIdForTesting(
        web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
        std::move(prefetch_service));

    browser_context()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext()
        ->GetCookieManager(cookie_manager_.BindNewPipeAndPassReceiver());

    auto navigation_simulator = NavigationSimulator::CreateBrowserInitiated(
        GURL("https://test.com"), web_contents());
    navigation_simulator->Start();

    interceptor_ = std::make_unique<TestPrefetchURLLoaderInterceptor>(
        web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId(),
        MainDocumentToken());

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    attempt_entry_builder_ =
        std::make_unique<test::PreloadingAttemptUkmEntryBuilder>(
            content_preloading_predictor::kSpeculationRules);

    scoped_test_timer_ =
        std::make_unique<base::ScopedMockElapsedTimersForTest>();
  }

  void TearDown() override {
    interceptor_.release();

    scoped_feature_list_.Reset();
    RenderViewHostTestHarness::TearDown();
  }

  TestPrefetchService* GetPrefetchService() {
    return static_cast<TestPrefetchService*>(
        PrefetchService::GetFromFrameTreeNodeId(
            web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId()));
  }

  TestPrefetchURLLoaderInterceptor* interceptor() { return interceptor_.get(); }

  void WaitForCallback(const GURL& url) {
    auto itr = was_intercepted_.find(url);
    if (itr != was_intercepted_.end()) {
      return;
    }

    base::RunLoop run_loop;
    on_loader_callback_closure_[url] = run_loop.QuitClosure();
    run_loop.Run();
  }

  void LoaderCallback(
      const GURL& url,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    was_intercepted_[url] = url_loader_factory != nullptr;

    auto itr = on_loader_callback_closure_.find(url);
    if (itr != on_loader_callback_closure_.end() && itr->second) {
      std::move(itr->second).Run();
    }
  }

  absl::optional<bool> was_intercepted(const GURL& url) {
    if (was_intercepted_.find(url) == was_intercepted_.end()) {
      return absl::nullopt;
    }
    return was_intercepted_[url];
  }

  NavigationRequest* navigation_request() {
    return FrameTreeNode::GloballyFindByID(
               web_contents()->GetPrimaryMainFrame()->GetFrameTreeNodeId())
        ->navigation_request();
  }

  bool SetCookie(const GURL& url, const std::string& value) {
    bool result = false;
    base::RunLoop run_loop;

    std::unique_ptr<net::CanonicalCookie> cookie(net::CanonicalCookie::Create(
        url, value, base::Time::Now(), /*server_time=*/absl::nullopt,
        /*cookie_partition_key=*/absl::nullopt));
    EXPECT_TRUE(cookie.get());
    EXPECT_TRUE(cookie->IsHostCookie());

    net::CookieOptions options;
    options.set_include_httponly();
    options.set_same_site_cookie_context(
        net::CookieOptions::SameSiteCookieContext::MakeInclusive());

    cookie_manager_->SetCanonicalCookie(
        *cookie.get(), url, options,
        base::BindOnce(
            [](bool* result, base::RunLoop* run_loop,
               net::CookieAccessResult set_cookie_access_result) {
              *result = set_cookie_access_result.status.IsInclude();
              run_loop->Quit();
            },
            &result, &run_loop));

    // This will run until the cookie is set.
    run_loop.Run();

    // This will run until the cookie listener gets the cookie change.
    task_environment()->RunUntilIdle();

    return result;
  }

  network::mojom::CookieManager* cookie_manager() {
    return cookie_manager_.get();
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

  ScopedMockContentBrowserClient* test_content_browser_client() {
    return test_content_browser_client_.get();
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  const test::PreloadingAttemptUkmEntryBuilder* attempt_entry_builder() {
    return attempt_entry_builder_.get();
  }

  void ExpectCorrectUkmLogs(const GURL& expected_url,
                            bool is_accurate_trigger,
                            PreloadingTriggeringOutcome expected_outcome,
                            PreloadingFailureReason expected_failure_reason =
                                PreloadingFailureReason::kUnspecified) {
    MockNavigationHandle mock_handle;
    mock_handle.set_is_in_primary_main_frame(true);
    mock_handle.set_is_same_document(false);
    mock_handle.set_has_committed(true);
    mock_handle.set_url(expected_url);
    auto* preloading_data =
        PreloadingData::GetOrCreateForWebContents(web_contents());

    auto* preloading_data_impl =
        static_cast<PreloadingDataImpl*>(preloading_data);
    preloading_data_impl->DidStartNavigation(&mock_handle);
    preloading_data_impl->DidFinishNavigation(&mock_handle);

    auto actual_attempts = test_ukm_recorder()->GetEntries(
        ukm::builders::Preloading_Attempt::kEntryName,
        test::kPreloadingAttemptUkmMetrics);
    EXPECT_EQ(actual_attempts.size(), 1u);

    const auto expected_attempts = {attempt_entry_builder()->BuildEntry(
        mock_handle.GetNextPageUkmSourceId(), PreloadingType::kPrefetch,
        PreloadingEligibility::kEligible, PreloadingHoldbackStatus::kAllowed,
        expected_outcome, expected_failure_reason,
        /*accurate=*/is_accurate_trigger,
        /*ready_time=*/base::ScopedMockElapsedTimersForTest::kMockElapsedTime,
        blink::mojom::SpeculationEagerness::kEager)};

    EXPECT_THAT(actual_attempts,
                testing::UnorderedElementsAreArray(expected_attempts))
        << test::ActualVsExpectedUkmEntriesToString(actual_attempts,
                                                    expected_attempts);
    // We do not test the `PreloadingPrediction` as it is added in
    // `PreloadingDecider`.
  }

  blink::DocumentToken MainDocumentToken() {
    return static_cast<RenderFrameHostImpl*>(main_rfh())->GetDocumentToken();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  std::unique_ptr<TestPrefetchURLLoaderInterceptor> interceptor_;

  base::HistogramTester histogram_tester_;

  std::map<GURL, bool> was_intercepted_;
  std::map<GURL, base::OnceClosure> on_loader_callback_closure_;

  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
  std::unique_ptr<ScopedMockContentBrowserClient> test_content_browser_client_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  std::unique_ptr<test::PreloadingAttemptUkmEntryBuilder>
      attempt_entry_builder_;

  std::unique_ptr<base::ScopedMockElapsedTimersForTest> scoped_test_timer_;
  // Disable sampling of UKM preloading logs.
  content::test::PreloadingConfigOverride preloading_config_override_;
};

class PrefetchURLLoaderInterceptorTest
    : public PrefetchURLLoaderInterceptorTestBase,
      public ::testing::WithParamInterface<PrefetchReusableForTests> {
  void SetUp() override {
    PrefetchURLLoaderInterceptorTestBase::SetUp();
    switch (GetParam()) {
      case PrefetchReusableForTests::kDisabled:
        scoped_feature_list_.InitAndDisableFeature(features::kPrefetchReusable);
        break;
      case PrefetchReusableForTests::kEnabled:
        scoped_feature_list_.InitAndEnableFeature(features::kPrefetchReusable);
        break;
    }
  }
};

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(InterceptNavigationCookieCopyCompleted)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          testing::ResultOf(
              [](const url::Origin& request_initiator) {
                return request_initiator.opaque();
              },
              true),
          testing::Optional(navigation_request()->GetNavigationId()),
          ukm::SourceIdObj::FromInt64(
              navigation_request()->GetNextPageUkmSourceId()),
          testing::NotNull(), testing::IsNull(), testing::NotNull(),
          testing::IsNull(), testing::IsNull(), testing::IsNull()))
      .WillOnce(testing::Return(false));

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), MainDocumentToken(), kTestUrl,
          PrefetchType(/*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kEager),
          blink::mojom::Referrer(),
          /*no_vary_search_expected=*/absl::nullopt,
          blink::mojom::SpeculationInjectionWorld::kNone,
          /*prefetch_document_manager=*/nullptr);
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  // Simulate the cookie copy process starting and finishing before
  // |MaybeCreateLoader| is called.
  auto reader = prefetch_container->CreateReader();
  reader.OnIsolatedCookieCopyStart();
  task_environment()->FastForwardBy(base::Milliseconds(10));
  reader.OnIsolatedCookieCopyComplete();

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kTestUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", base::TimeDelta(),
      1);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  EXPECT_EQ(prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchResponseUsed);
  ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kSuccess);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(InterceptNavigationCookieCopyInProgress)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          testing::ResultOf(
              [](const url::Origin& request_initiator) {
                return request_initiator.opaque();
              },
              true),
          testing::Optional(navigation_request()->GetNavigationId()),
          ukm::SourceIdObj::FromInt64(
              navigation_request()->GetNextPageUkmSourceId()),
          testing::NotNull(), testing::IsNull(), testing::NotNull(),
          testing::IsNull(), testing::IsNull(), testing::IsNull()))
      .WillOnce(testing::Return(false));

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), MainDocumentToken(), kTestUrl,
          PrefetchType(/*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kEager),
          blink::mojom::Referrer(),
          /*no_vary_search_expected=*/absl::nullopt,
          blink::mojom::SpeculationInjectionWorld::kNone,
          /*prefetch_document_manager=*/nullptr);
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  // Simulate the cookie copy process starting, but not finishing until after
  // |MaybeCreateLoader| is called.
  auto reader = prefetch_container->CreateReader();
  reader.OnIsolatedCookieCopyStart();
  task_environment()->FastForwardBy(base::Milliseconds(10));

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kTestUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));

  // A decision on whether the navigation should be intercepted shouldn't be
  // made until after the cookie copy process is completed.
  EXPECT_FALSE(was_intercepted(kTestUrl).has_value());

  task_environment()->FastForwardBy(base::Milliseconds(20));

  reader.OnIsolatedCookieCopyComplete();
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime",
      base::Milliseconds(20), 1);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kSuccess);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(InterceptNavigationNoCookieCopyNeeded)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          testing::ResultOf(
              [](const url::Origin& request_initiator) {
                return request_initiator.opaque();
              },
              true),
          testing::Optional(navigation_request()->GetNavigationId()),
          ukm::SourceIdObj::FromInt64(
              navigation_request()->GetNextPageUkmSourceId()),
          testing::NotNull(), testing::IsNull(), testing::NotNull(),
          testing::IsNull(), testing::IsNull(), testing::IsNull()))
      .WillOnce(testing::Return(false));

  // No cookies are copied for prefetches where |use_isolated_network_context|
  // is false (i.e. same origin prefetches).
  blink::mojom::Referrer referrer;
  referrer.url = GURL("https://example.com/referrer");
  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), MainDocumentToken(), kTestUrl,
          PrefetchType(/*use_prefetch_proxy=*/false,
                       blink::mojom::SpeculationEagerness::kEager),
          referrer,
          /*no_vary_search_expected=*/absl::nullopt,
          blink::mojom::SpeculationInjectionWorld::kNone,
          /*prefetch_document_manager=*/nullptr);
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kTestUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", base::TimeDelta(),
      1);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kSuccess);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotInterceptNavigationNoPrefetch)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(*test_content_browser_client(), WillCreateURLLoaderFactory)
      .Times(0);

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  // With no prefetch set, the navigation shouldn't be intercepted.

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kTestUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_FALSE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);

  auto actual = test_ukm_recorder()->GetEntries(
      ukm::builders::Preloading_Attempt::kEntryName,
      test::kPreloadingAttemptUkmMetrics);
  EXPECT_EQ(actual.size(), 0u);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotInterceptNavigationNoPrefetchedResponse)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(*test_content_browser_client(), WillCreateURLLoaderFactory)
      .Times(0);

  // Without a prefetched response, the navigation shouldn't be intercepted.
  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), MainDocumentToken(), kTestUrl,
          PrefetchType(/*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kEager),
          blink::mojom::Referrer(),
          /*no_vary_search_expected=*/absl::nullopt,
          blink::mojom::SpeculationInjectionWorld::kNone,
          /*prefetch_document_manager=*/nullptr);
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  // Set up ResourceRequest
  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  // Try to create loader
  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kTestUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_FALSE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  ExpectCorrectUkmLogs(GURL("http://Not.Accurate.Trigger/"),
                       /*is_accurate_trigger=*/false,
                       PreloadingTriggeringOutcome::kReady);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotInterceptNavigationStalePrefetchedResponse)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(*test_content_browser_client(), WillCreateURLLoaderFactory)
      .Times(0);

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), MainDocumentToken(), kTestUrl,
          PrefetchType(/*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kEager),
          blink::mojom::Referrer(),
          /*no_vary_search_expected=*/absl::nullopt,
          blink::mojom::SpeculationInjectionWorld::kNone,
          /*prefetch_document_manager=*/nullptr);
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  // Advance time enough so that the response is considered stale.
  task_environment()->FastForwardBy(2 * PrefetchCacheableDuration());

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kTestUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_FALSE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  ExpectCorrectUkmLogs(GURL("http://Not.Accurate.Trigger/"),
                       /*is_accurate_trigger=*/false,
                       PreloadingTriggeringOutcome::kReady);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(DoNotInterceptNavigationCookiesChanged)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(*test_content_browser_client(), WillCreateURLLoaderFactory)
      .Times(0);

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), MainDocumentToken(), kTestUrl,
          PrefetchType(/*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kEager),
          blink::mojom::Referrer(),
          /*no_vary_search_expected=*/absl::nullopt,
          blink::mojom::SpeculationInjectionWorld::kNone,
          /*prefetch_document_manager=*/nullptr);
  prefetch_container->RegisterCookieListener(cookie_manager());
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  // Since the cookies associated with |kTestUrl| have changed, the prefetch can
  // no longer be served.
  ASSERT_TRUE(SetCookie(kTestUrl, "test-cookie"));

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kTestUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_FALSE(was_intercepted(kTestUrl).value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 0);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  ExpectCorrectUkmLogs(GURL("http://Not.Accurate.Trigger/"),
                       /*is_accurate_trigger=*/false,
                       PreloadingTriggeringOutcome::kFailure,
                       ToPreloadingFailureReason(
                           PrefetchStatus::kPrefetchNotUsedCookiesChanged));
}

TEST_P(PrefetchURLLoaderInterceptorTest, DISABLE_ASAN(ProbeSuccess)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          testing::ResultOf(
              [](const url::Origin& request_initiator) {
                return request_initiator.opaque();
              },
              true),
          testing::Optional(navigation_request()->GetNavigationId()),
          ukm::SourceIdObj::FromInt64(
              navigation_request()->GetNextPageUkmSourceId()),
          testing::NotNull(), testing::IsNull(), testing::NotNull(),
          testing::IsNull(), testing::IsNull(), testing::IsNull()))
      .WillOnce(testing::Return(false));

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), MainDocumentToken(), kTestUrl,
          PrefetchType(/*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kEager),
          blink::mojom::Referrer(),
          /*no_vary_search_expected=*/absl::nullopt,
          blink::mojom::SpeculationInjectionWorld::kNone,
          /*prefetch_document_manager=*/nullptr);
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  auto reader = prefetch_container->CreateReader();
  reader.OnIsolatedCookieCopyStart();
  reader.OnIsolatedCookieCopyComplete();

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  // Set up |TestPrefetchOriginProber| to require a probe and simulate a
  // successful probe.
  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/true, kTestUrl,
          PrefetchProbeResult::kDNSProbeSuccess));

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kTestUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());

  EXPECT_EQ(GetPrefetchService()->num_probes(), 1);
  ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kSuccess);
}

TEST_P(PrefetchURLLoaderInterceptorTest, DISABLE_ASAN(ProbeFailure)) {
  const GURL kTestUrl("https://example.com");

  EXPECT_CALL(*test_content_browser_client(), WillCreateURLLoaderFactory)
      .Times(0);

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), MainDocumentToken(), kTestUrl,
          PrefetchType(/*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kEager),
          blink::mojom::Referrer(),
          /*no_vary_search_expected=*/absl::nullopt,
          blink::mojom::SpeculationInjectionWorld::kNone,
          /*prefetch_document_manager=*/nullptr);
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderForTest(prefetch_container.get(),
                                        network::mojom::URLResponseHead::New(),
                                        "test body");

  auto reader = prefetch_container->CreateReader();
  reader.OnIsolatedCookieCopyStart();
  reader.OnIsolatedCookieCopyComplete();

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  // Set up |TestPrefetchOriginProber| to require a probe and simulate a
  // unsuccessful probe.
  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/true, kTestUrl,
          PrefetchProbeResult::kDNSProbeFailure));

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kTestUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_FALSE(was_intercepted(kTestUrl).value());

  EXPECT_EQ(GetPrefetchService()->num_probes(), 1);
  ExpectCorrectUkmLogs(GURL("http://Not.Accurate.Trigger/"),
                       /*is_accurate_trigger=*/false,
                       PreloadingTriggeringOutcome::kReady);
}

enum class NotServableReason {
  kOnCompleteFailure,
  kAnotherRequest,
  kAnotherRequestCompleted,
};

class PrefetchURLLoaderInterceptorBecomeNotServableTest
    : public PrefetchURLLoaderInterceptorTestBase,
      public ::testing::WithParamInterface<
          std::tuple<PrefetchReusableForTests, NotServableReason>> {
  void SetUp() override {
    PrefetchURLLoaderInterceptorTestBase::SetUp();
    switch (std::get<0>(GetParam())) {
      case PrefetchReusableForTests::kDisabled:
        scoped_feature_list_.InitAndDisableFeature(features::kPrefetchReusable);
        break;
      case PrefetchReusableForTests::kEnabled:
        scoped_feature_list_.InitAndEnableFeature(features::kPrefetchReusable);
        break;
    }
  }
};

TEST_P(PrefetchURLLoaderInterceptorBecomeNotServableTest, DISABLE_ASAN(Basic)) {
  // It is possible for a prefetch to initially be marked as servable, but
  // becomes not servable at some point between PrefetchURLLoaderInterceptor
  // gets the prefetch and when it tries to serve it. This can happen when
  // waiting for a probe to complete or the cookie copy to complete.

  const GURL kTestUrl("https://example.com");

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), MainDocumentToken(), kTestUrl,
          PrefetchType(/*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kEager),
          blink::mojom::Referrer(),
          /*no_vary_search_expected=*/absl::nullopt,
          blink::mojom::SpeculationInjectionWorld::kNone,
          /*prefetch_document_manager=*/nullptr);
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  auto pending_request =
      MakeManuallyServableStreamingURLLoaderForTest(prefetch_container.get());

  mojo::ScopedDataPipeProducerHandle producer_handle;
  {
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    std::string content = "test body";
    CHECK_EQ(
        mojo::CreateDataPipe(content.size(), producer_handle, consumer_handle),
        MOJO_RESULT_OK);
    uint32_t bytes_written = content.size();
    CHECK_EQ(MOJO_RESULT_OK,
             producer_handle->WriteData(content.data(), &bytes_written,
                                        MOJO_WRITE_DATA_FLAG_ALL_OR_NONE));
    pending_request.client->OnReceiveResponse(
        network::mojom::URLResponseHead::New(), std::move(consumer_handle),
        absl::nullopt);
  }

  // Simulate the cookie copy process starting, but not finishing until after
  // |MaybeCreateLoader| is called.
  auto reader = prefetch_container->CreateReader();
  reader.OnIsolatedCookieCopyStart();
  task_environment()->FastForwardBy(base::Milliseconds(10));

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  network::ResourceRequest request;
  request.url = kTestUrl;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request.method = "GET";

  interceptor()->MaybeCreateLoader(
      request, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kTestUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));

  // A decision on whether the navigation should be intercepted shouldn't be
  // made until after the cookie copy process is completed.
  EXPECT_FALSE(was_intercepted(kTestUrl).has_value());

  task_environment()->FastForwardBy(base::Milliseconds(20));

  // Simulate the prefetch becoming not servable anymore.
  PrefetchRequestHandler another_request;
  switch (std::get<1>(GetParam())) {
    case NotServableReason::kOnCompleteFailure:
      producer_handle.reset();
      pending_request.client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_FAILED));
      break;

    case NotServableReason::kAnotherRequest:
      // Another request is created for the same PrefetchContainer while
      // prefetching is still ongoing.
      another_request =
          prefetch_container->CreateReader().CreateRequestHandler();
      break;

    case NotServableReason::kAnotherRequestCompleted:
      // Another request is created for the same PrefetchContainer while
      // prefetching is still ongoing,
      another_request =
          prefetch_container->CreateReader().CreateRequestHandler();

      // and, prefetch and the other request completed.
      {
        producer_handle.reset();
        pending_request.client->OnComplete(
            network::URLLoaderCompletionStatus(net::OK));

        std::unique_ptr<PrefetchTestURLLoaderClient> client =
            std::make_unique<PrefetchTestURLLoaderClient>();

        std::move(another_request)
            .Run(request, client->BindURLloaderAndGetReceiver(),
                 client->BindURLLoaderClientAndGetRemote());
        // Wait until the URLLoaderClient completion.
        task_environment()->RunUntilIdle();
        EXPECT_EQ(client->body_content(), "test body");
        client->DisconnectMojoPipes();
      }
      break;
  }

  task_environment()->RunUntilIdle();

  reader.OnIsolatedCookieCopyComplete();
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());

  switch (std::get<1>(GetParam())) {
    case NotServableReason::kOnCompleteFailure:
      EXPECT_FALSE(was_intercepted(kTestUrl).value());
      ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                           PreloadingTriggeringOutcome::kReady);
      break;

    case NotServableReason::kAnotherRequest:
      EXPECT_FALSE(was_intercepted(kTestUrl).value());
      ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                           PreloadingTriggeringOutcome::kSuccess);
      producer_handle.reset();
      pending_request.client->OnComplete(
          network::URLLoaderCompletionStatus(net::OK));
      task_environment()->RunUntilIdle();
      break;

    case NotServableReason::kAnotherRequestCompleted:
      switch (std::get<0>(GetParam())) {
        case PrefetchReusableForTests::kDisabled:
          EXPECT_FALSE(was_intercepted(kTestUrl).value());
          break;
        case PrefetchReusableForTests::kEnabled:
          // The first request doesn't become non-servable if
          // `kPrefetchReusable` is enabled, because after the other
          // request is done, the body tee is clonable again.
          EXPECT_TRUE(was_intercepted(kTestUrl).value());
          break;
      }
      ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                           PreloadingTriggeringOutcome::kSuccess);
      break;
  }

  histogram_tester().ExpectUniqueTimeSample(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime",
      base::Milliseconds(20), 1);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PrefetchURLLoaderInterceptorBecomeNotServableTest,
    testing::Combine(testing::ValuesIn(PrefetchReusableValuesForTests()),
                     testing::Values(NotServableReason::kOnCompleteFailure,
                                     NotServableReason::kAnotherRequest)));

TEST_P(PrefetchURLLoaderInterceptorTest, DISABLE_ASAN(HandleRedirects)) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPrefetchRedirects);

  const GURL kTestUrl("https://example.com");
  const GURL kRedirectUrl("https://redirect.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          testing::ResultOf(
              [](const url::Origin& request_initiator) {
                return request_initiator.opaque();
              },
              true),
          testing::Optional(navigation_request()->GetNavigationId()),
          ukm::SourceIdObj::FromInt64(
              navigation_request()->GetNextPageUkmSourceId()),
          testing::NotNull(), testing::IsNull(), testing::NotNull(),
          testing::IsNull(), testing::IsNull(), testing::IsNull()))
      .Times(2)
      .WillRepeatedly(testing::Return(false));

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), MainDocumentToken(), kTestUrl,
          PrefetchType(/*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kEager),
          blink::mojom::Referrer(),
          /*no_vary_search_expected=*/absl::nullopt,
          blink::mojom::SpeculationInjectionWorld::kNone,
          /*prefetch_document_manager=*/nullptr);
  prefetch_container->MakeResourceRequest({});
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoaderWithRedirectForTest(prefetch_container.get(),
                                                    kTestUrl, kRedirectUrl);

  // Simulate the cookie copy process starting and finishing before
  // |MaybeCreateLoader| is called.
  auto reader = prefetch_container->CreateReader();
  reader.OnIsolatedCookieCopyStart();
  task_environment()->FastForwardBy(base::Milliseconds(10));
  reader.OnIsolatedCookieCopyComplete();

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  network::ResourceRequest request1;
  request1.url = kTestUrl;
  request1.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request1.method = "GET";

  interceptor()->MaybeCreateLoader(
      request1, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kTestUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());
  EXPECT_FALSE(was_intercepted(kRedirectUrl).has_value());

  base::RunLoop on_start_cookie_copy_run_loop;
  GetPrefetchService()->AddOnStartCookieCopyClosure(
      kTestUrl, kRedirectUrl, on_start_cookie_copy_run_loop.QuitClosure());

  network::ResourceRequest request2;
  request2.url = kRedirectUrl;
  request2.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request2.method = "GET";

  interceptor()->MaybeCreateLoader(
      request2, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kRedirectUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));

  on_start_cookie_copy_run_loop.Run();
  task_environment()->FastForwardBy(base::Milliseconds(20));
  reader.AdvanceCurrentURLToServe();
  reader.OnIsolatedCookieCopyComplete();
  WaitForCallback(kRedirectUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());
  EXPECT_TRUE(was_intercepted(kRedirectUrl).has_value());
  EXPECT_TRUE(was_intercepted(kRedirectUrl).value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 2);
  histogram_tester().ExpectTimeBucketCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", base::TimeDelta(),
      1);
  histogram_tester().ExpectTimeBucketCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime",
      base::Milliseconds(20), 1);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  EXPECT_EQ(prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchResponseUsed);
  ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kSuccess);
}

TEST_P(PrefetchURLLoaderInterceptorTest,
       DISABLE_ASAN(HandleRedirectsWithSwitchInNetworkContext)) {
  base::test::ScopedFeatureList scoped_feature_list(
      features::kPrefetchRedirects);
  const GURL kTestUrl("https://example.com");
  const GURL kRedirectUrl("https://redirect.com");

  EXPECT_CALL(
      *test_content_browser_client(),
      WillCreateURLLoaderFactory(
          testing::NotNull(), main_rfh(), main_rfh()->GetProcess()->GetID(),
          ContentBrowserClient::URLLoaderFactoryType::kNavigation,
          testing::ResultOf(
              [](const url::Origin& request_initiator) {
                return request_initiator.opaque();
              },
              true),
          testing::Optional(navigation_request()->GetNavigationId()),
          ukm::SourceIdObj::FromInt64(
              navigation_request()->GetNextPageUkmSourceId()),
          testing::NotNull(), testing::IsNull(), testing::NotNull(),
          testing::IsNull(), testing::IsNull(), testing::IsNull()))
      .Times(2)
      .WillRepeatedly(testing::Return(false));

  blink::mojom::Referrer referrer;
  referrer.url = GURL("https://example.com/referrer");

  std::unique_ptr<PrefetchContainer> prefetch_container =
      std::make_unique<PrefetchContainer>(
          main_rfh()->GetGlobalId(), MainDocumentToken(), kTestUrl,
          PrefetchType(/*use_prefetch_proxy=*/true,
                       blink::mojom::SpeculationEagerness::kEager),
          referrer,
          /*no_vary_search_expected=*/absl::nullopt,
          blink::mojom::SpeculationInjectionWorld::kNone,
          /*prefetch_document_manager=*/nullptr);
  prefetch_container->MakeResourceRequest({});
  prefetch_container->SimulateAttemptAtInterceptorForTest();

  MakeServableStreamingURLLoadersWithNetworkTransitionRedirectForTest(
      prefetch_container.get(), kTestUrl, kRedirectUrl);

  interceptor()->AddPrefetch(prefetch_container->GetWeakPtr());

  GetPrefetchService()->TakePrefetchOriginProber(
      std::make_unique<TestPrefetchOriginProber>(
          browser_context(), /*should_probe_origins_response=*/false, kTestUrl,
          PrefetchProbeResult::kNoProbing));

  network::ResourceRequest request1;
  request1.url = kTestUrl;
  request1.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request1.method = "GET";

  interceptor()->MaybeCreateLoader(
      request1, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kTestUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));
  WaitForCallback(kTestUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());
  EXPECT_FALSE(was_intercepted(kRedirectUrl).has_value());

  base::RunLoop on_start_cookie_copy_run_loop;
  GetPrefetchService()->AddOnStartCookieCopyClosure(
      kTestUrl, kRedirectUrl, on_start_cookie_copy_run_loop.QuitClosure());

  network::ResourceRequest request2;
  request2.url = kRedirectUrl;
  request2.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kMainFrame);
  request2.method = "GET";

  interceptor()->MaybeCreateLoader(
      request2, browser_context(),
      base::BindOnce(&PrefetchURLLoaderInterceptorTest::LoaderCallback,
                     base::Unretained(this), kRedirectUrl),
      base::BindOnce([](bool, const net::LoadTimingInfo&) { NOTREACHED(); }));

  auto reader = prefetch_container->CreateReader();
  on_start_cookie_copy_run_loop.Run();
  task_environment()->FastForwardBy(base::Milliseconds(20));
  reader.AdvanceCurrentURLToServe();
  reader.OnIsolatedCookieCopyComplete();
  WaitForCallback(kRedirectUrl);

  EXPECT_TRUE(was_intercepted(kTestUrl).has_value());
  EXPECT_TRUE(was_intercepted(kTestUrl).value());
  EXPECT_TRUE(was_intercepted(kRedirectUrl).has_value());
  EXPECT_TRUE(was_intercepted(kRedirectUrl).value());

  histogram_tester().ExpectTotalCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", 2);
  histogram_tester().ExpectTimeBucketCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime", base::TimeDelta(),
      1);
  histogram_tester().ExpectTimeBucketCount(
      "PrefetchProxy.AfterClick.Mainframe.CookieWaitTime",
      base::Milliseconds(20), 1);

  EXPECT_EQ(GetPrefetchService()->num_probes(), 0);
  EXPECT_EQ(prefetch_container->GetPrefetchStatus(),
            PrefetchStatus::kPrefetchResponseUsed);
  ExpectCorrectUkmLogs(kTestUrl, /*is_accurate_trigger=*/true,
                       PreloadingTriggeringOutcome::kSuccess);
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    PrefetchURLLoaderInterceptorTest,
    testing::ValuesIn(PrefetchReusableValuesForTests()));

}  // namespace
}  // namespace content

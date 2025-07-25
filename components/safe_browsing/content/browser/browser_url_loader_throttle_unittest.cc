// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_utils.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

using security_interstitials::UnsafeResource;

class MockUrlCheckerDelegate : public UrlCheckerDelegate {
 public:
  MockUrlCheckerDelegate() = default;

  MOCK_METHOD1(MaybeDestroyNoStatePrefetchContents,
               void(base::OnceCallback<content::WebContents*()>));
  MOCK_METHOD5(StartDisplayingBlockingPageHelper,
               void(const security_interstitials::UnsafeResource&,
                    const std::string&,
                    const net::HttpRequestHeaders&,
                    bool,
                    bool));
  MOCK_METHOD2(StartObservingInteractionsForDelayedBlockingPageHelper,
               void(const security_interstitials::UnsafeResource&, bool));
  MOCK_METHOD1(NotifySuspiciousSiteDetected,
               void(const base::RepeatingCallback<content::WebContents*()>&));
  MOCK_METHOD0(GetUIManager, BaseUIManager*());
  MOCK_METHOD0(GetThreatTypes, const SBThreatTypeSet&());
  MOCK_METHOD1(IsUrlAllowlisted, bool(const GURL&));
  MOCK_METHOD1(SetPolicyAllowlistDomains,
               void(const std::vector<std::string>&));
  MOCK_METHOD3(CheckLookupMechanismExperimentEligibility,
               void(const security_interstitials::UnsafeResource&,
                    base::OnceCallback<void(bool)>,
                    scoped_refptr<base::SequencedTaskRunner>));
  MOCK_METHOD3(CheckExperimentEligibilityAndStartBlockingPage,
               void(const security_interstitials::UnsafeResource&,
                    base::OnceCallback<void(bool)>,
                    scoped_refptr<base::SequencedTaskRunner>));

  SafeBrowsingDatabaseManager* GetDatabaseManager() override { return nullptr; }

  bool ShouldSkipRequestCheck(const GURL& original_url,
                              int frame_tree_node_id,
                              int render_process_id,
                              int render_frame_id,
                              bool originated_from_service_worker) override {
    return should_skip_request_check_;
  }
  void EnableSkipRequestCheck() { should_skip_request_check_ = true; }

 protected:
  ~MockUrlCheckerDelegate() override = default;

 private:
  bool should_skip_request_check_ = false;
};

class MockThrottleDelegate : public blink::URLLoaderThrottle::Delegate {
 public:
  ~MockThrottleDelegate() override = default;

  void CancelWithError(int error_code,
                       base::StringPiece custom_reason) override {
    error_code_ = error_code;
    custom_reason_ = custom_reason;
  }
  void Resume() override { is_resumed_ = true; }

  int GetErrorCode() { return error_code_; }
  base::StringPiece GetCustomReason() { return custom_reason_; }
  bool IsResumed() { return is_resumed_; }

 private:
  int error_code_ = 0;
  base::StringPiece custom_reason_ = "";
  bool is_resumed_ = false;
};

class MockRealTimeUrlLookupService : public RealTimeUrlLookupServiceBase {
 public:
  MockRealTimeUrlLookupService()
      : RealTimeUrlLookupServiceBase(
            /*url_loader_factory=*/nullptr,
            /*cache_manager=*/nullptr,
            /*get_user_population_callback=*/base::BindRepeating([]() {
              return ChromeUserPopulation();
            }),
            /*referrer_chain_provider=*/nullptr,
            /*pref_service=*/nullptr) {}

  // RealTimeUrlLookupServiceBase:
  bool CanPerformFullURLLookup() const override { return true; }
  bool CanCheckSubresourceURL() const override { return false; }
  bool CanCheckSafeBrowsingDb() const override { return true; }
  bool CanCheckSafeBrowsingHighConfidenceAllowlist() const override {
    return true;
  }
  bool CanSendRTSampleRequest() const override { return false; }
  std::string GetMetricSuffix() const override { return ".Mock"; }
  void StartLookup(
      const GURL& url,
      const GURL& last_committed_url,
      bool is_mainframe,
      RTLookupRequestCallback request_callback,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {}
  void SendSampledRequest(
      const GURL& url,
      const GURL& last_committed_url,
      bool is_mainframe,
      RTLookupRequestCallback request_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {}

 private:
  GURL GetRealTimeLookupUrl() const override { return GURL(); }
  net::NetworkTrafficAnnotationTag GetTrafficAnnotationTag() const override {
    return TRAFFIC_ANNOTATION_FOR_TESTS;
  }
  bool CanPerformFullURLLookupWithToken() const override { return false; }
  int GetReferrerUserGestureLimit() const override { return 0; }
  bool CanSendPageLoadToken() const override { return false; }
  void GetAccessToken(
      const GURL& url,
      const GURL& last_committed_url,
      bool is_mainframe,
      RTLookupRequestCallback request_callback,
      RTLookupResponseCallback response_callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner) override {}
  absl::optional<std::string> GetDMTokenString() const override {
    return absl::nullopt;
  }
  bool ShouldIncludeCredentials() const override { return false; }
  double GetMinAllowedTimestampForReferrerChains() const override { return 0; }
};

class MockSafeBrowsingUrlChecker : public SafeBrowsingUrlCheckerImpl {
 public:
  struct CallbackInfo {
    bool should_proceed = true;
    bool should_show_interstitial = false;
    bool should_delay_callback = false;
    NativeCheckUrlCallback callback;
    PerformedCheck performed_check = PerformedCheck::kHashDatabaseCheck;
  };

  MockSafeBrowsingUrlChecker(
      const net::HttpRequestHeaders& headers,
      int load_flags,
      network::mojom::RequestDestination request_destination,
      bool has_user_gesture,
      scoped_refptr<UrlCheckerDelegate> url_checker_delegate,
      const base::RepeatingCallback<content::WebContents*()>&
          web_contents_getter,
      UnsafeResource::RenderProcessId render_process_id,
      UnsafeResource::RenderFrameId render_frame_id,
      UnsafeResource::FrameTreeNodeId frame_tree_node_id,
      bool url_real_time_lookup_enabled,
      bool can_urt_check_subresource_url,
      bool can_check_db,
      bool can_check_high_confidence_allowlist,
      std::string url_lookup_service_metric_suffix,
      GURL last_committed_url,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service_on_ui,
      UrlRealTimeMechanism::WebUIDelegate* webui_delegate,
      base::WeakPtr<HashRealTimeService> hash_realtime_service_on_ui,
      scoped_refptr<SafeBrowsingLookupMechanismExperimenter>
          mechanism_experimenter,
      bool is_mechanism_experiment_allowed,
      hash_realtime_utils::HashRealTimeSelection hash_realtime_selection)
      : SafeBrowsingUrlCheckerImpl(headers,
                                   load_flags,
                                   request_destination,
                                   has_user_gesture,
                                   url_checker_delegate,
                                   web_contents_getter,
                                   render_process_id,
                                   render_frame_id,
                                   frame_tree_node_id,
                                   url_real_time_lookup_enabled,
                                   can_urt_check_subresource_url,
                                   can_check_db,
                                   can_check_high_confidence_allowlist,
                                   url_lookup_service_metric_suffix,
                                   last_committed_url,
                                   ui_task_runner,
                                   url_lookup_service_on_ui,
                                   webui_delegate,
                                   hash_realtime_service_on_ui,
                                   mechanism_experimenter,
                                   is_mechanism_experiment_allowed,
                                   hash_realtime_selection) {}

  // Returns the CallbackInfo that was previously added in |AddCallbackInfo|.
  // It will crash if |AddCallbackInfo| was not called.
  // If |should_delay_callback| was set to true, the callback will be cached.
  // The callback can be invoked manually via |RestartDelayedCallback|.
  // Otherwise, the callback will be invoked immediately.
  void CheckUrl(const GURL& url,
                const std::string& method,
                NativeCheckUrlCallback callback) override {
    ASSERT_GT(callback_infos_.size(), check_url_called_cnt_);
    CallbackInfo& callback_info = callback_infos_[check_url_called_cnt_++];
    if (callback_info.should_delay_callback) {
      callback_info.callback = std::move(callback);
    } else {
      std::move(callback).Run(
          /*slow_check_notifier=*/nullptr,
          /*proceed=*/callback_info.should_proceed,
          /*show_interstitial=*/
          callback_info.should_show_interstitial, callback_info.performed_check,
          /*did_check_url_real_time_allowlist=*/false);
    }
  }

  void RestartDelayedCallback(size_t index) {
    ASSERT_GT(callback_infos_.size(), index);
    ASSERT_TRUE(callback_infos_[index].should_delay_callback);
    std::move(callback_infos_[index].callback)
        .Run(/*slow_check_notifier=*/nullptr,
             /*proceed=*/callback_infos_[index].should_proceed,
             /*show_interstitial=*/
             callback_infos_[index].should_show_interstitial,
             callback_infos_[index].performed_check,
             /*did_check_url_real_time_allowlist=*/false);
  }

  // Informs how the callback in |CheckUrl| should be handled. The info applies
  // to the callback sent in |CheckUrl| in sequence. That is, the first info
  // added will be applied to the first call of |CheckUrl|.
  void AddCallbackInfo(
      bool should_proceed,
      bool should_show_interstitial,
      bool should_delay_callback,
      PerformedCheck performed_check = PerformedCheck::kHashDatabaseCheck) {
    CallbackInfo callback_info;
    callback_info.should_proceed = should_proceed;
    callback_info.should_show_interstitial = should_show_interstitial;
    callback_info.should_delay_callback = should_delay_callback;
    callback_info.performed_check = performed_check;
    callback_infos_.push_back(std::move(callback_info));
  }

 private:
  size_t check_url_called_cnt_ = 0;
  std::vector<CallbackInfo> callback_infos_;
};

}  // namespace

class SBBrowserUrlLoaderThrottleTest : public ::testing::Test {
 protected:
  SBBrowserUrlLoaderThrottleTest() {
    feature_list_.InitAndEnableFeature(kSafeBrowsingSkipSubresources);
  }

  scoped_refptr<UrlCheckerDelegate> GetUrlCheckerDelegate() {
    return url_checker_delegate_;
  }

  void SetUpTest(bool url_real_time_lookup_enabled = false) {
    auto url_checker_delegate_getter = base::BindOnce(
        [](SBBrowserUrlLoaderThrottleTest* test) {
          return test->GetUrlCheckerDelegate();
        },
        base::Unretained(this));
    base::MockCallback<base::RepeatingCallback<content::WebContents*()>>
        mock_web_contents_getter;
    EXPECT_CALL(mock_web_contents_getter, Run())
        .WillOnce(testing::Return(nullptr));
    throttle_ = BrowserURLLoaderThrottle::Create(
        std::move(url_checker_delegate_getter), mock_web_contents_getter.Get(),
        /*frame_tree_node_id=*/0,
        url_real_time_lookup_enabled ? url_lookup_service_->GetWeakPtr()
                                     : nullptr,
        /*hash_realtime_service=*/nullptr, /*ping_manager=*/nullptr,
        /*hash_realtime_selection=*/
        hash_realtime_utils::HashRealTimeSelection::kNone);

    url_checker_delegate_ = base::MakeRefCounted<MockUrlCheckerDelegate>();
    throttle_delegate_ = std::make_unique<MockThrottleDelegate>();

    std::unique_ptr<MockSafeBrowsingUrlChecker> url_checker =
        std::make_unique<MockSafeBrowsingUrlChecker>(
            net::HttpRequestHeaders(), /*load_flags=*/0,
            network::mojom::RequestDestination::kDocument,
            /*has_user_gesture=*/false, url_checker_delegate_,
            mock_web_contents_getter.Get(), UnsafeResource::kNoRenderProcessId,
            UnsafeResource::kNoRenderFrameId,
            UnsafeResource::kNoFrameTreeNodeId, url_real_time_lookup_enabled,
            /*can_urt_check_subresource_url=*/false, /*can_check_db=*/true,
            /*can_check_high_confidence_allowlist=*/true,
            /*url_lookup_service_metric_suffix=*/"",
            /*last_committed_url=*/GURL(),
            /*ui_task_runner=*/base::SequencedTaskRunner::GetCurrentDefault(),
            /*url_lookup_service_on_ui=*/nullptr,
            /*webui_delegate_=*/nullptr,
            /*hash_realtime_service_on_ui=*/nullptr,
            /*mechanism_experimenter=*/nullptr,
            /*is_mechanism_experiment_allowed=*/false,
            /*hash_realtime_selection=*/
            hash_realtime_utils::HashRealTimeSelection::kNone);
    url_checker_ = url_checker.get();

    throttle_->GetSBCheckerForTesting()->SetUrlCheckerForTesting(
        std::move(url_checker));
    throttle_->set_delegate(throttle_delegate_.get());

    url_ = GURL("https://example.com/");
    response_head_ = network::mojom::URLResponseHead::New();
  }

  // This function returns the value of |defer| after the function is called.
  bool CallWillStartRequestWithDestination(
      network::mojom::RequestDestination destination) {
    bool defer = false;
    network::ResourceRequest request;
    request.url = url_;
    request.destination = destination;
    throttle_->WillStartRequest(&request, &defer);
    task_environment_.RunUntilIdle();
    return defer;
  }

  bool CallWillStartRequest() {
    return CallWillStartRequestWithDestination(
        network::mojom::RequestDestination::kDocument);
  }

  // This function returns the value of |defer| after the function is called.
  bool CallWillRedirectRequest() {
    bool defer = false;
    net::RedirectInfo redirect_info;
    std::vector<std::string> to_be_removed_headers;
    net::HttpRequestHeaders modified_headers;
    net::HttpRequestHeaders modified_cors_exempt_headers;
    throttle_->WillRedirectRequest(&redirect_info, *response_head_, &defer,
                                   &to_be_removed_headers, &modified_headers,
                                   &modified_cors_exempt_headers);
    task_environment_.RunUntilIdle();
    return defer;
  }

  // This function returns the value of |defer| after the function is called.
  bool CallWillProcessResponse() {
    bool defer = false;
    throttle_->WillProcessResponse(url_, response_head_.get(), &defer);
    task_environment_.RunUntilIdle();
    return defer;
  }

  // This function returns the value of |defer| after the function is called.
  bool CallWillProcessResponseFromCache() {
    bool defer = false;
    response_head_->was_fetched_via_cache = true;
    response_head_->network_accessed = false;
    throttle_->WillProcessResponse(url_, response_head_.get(), &defer);
    task_environment_.RunUntilIdle();
    return defer;
  }

  void RunTotalDelayHistogramsUrlCheckTypeTest(
      SafeBrowsingUrlCheckerImpl::PerformedCheck performed_check,
      bool url_real_time_lookup_enabled,
      std::string expected_histogram) {
    SetUpTest(url_real_time_lookup_enabled);
    base::HistogramTester histograms;
    url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                  /*should_show_interstitial=*/false,
                                  /*should_delay_callback=*/true,
                                  performed_check);

    CallWillStartRequest();
    CallWillProcessResponse();
    task_environment_.FastForwardBy(base::Milliseconds(200));
    url_checker_->RestartDelayedCallback(/*index=*/0);
    task_environment_.RunUntilIdle();

    histograms.ExpectUniqueTimeSample(expected_histogram,
                                      base::Milliseconds(200), 1);
  }

  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  GURL url_;
  network::mojom::URLResponseHeadPtr response_head_;
  std::unique_ptr<BrowserURLLoaderThrottle> throttle_;
  // Owned by |throttle_|. May be deleted before the test completes. Prefer
  // setting it up at the start of the test.
  raw_ptr<MockSafeBrowsingUrlChecker, AcrossTasksDanglingUntriaged>
      url_checker_;
  std::unique_ptr<MockRealTimeUrlLookupService> url_lookup_service_ =
      std::make_unique<MockRealTimeUrlLookupService>();
  scoped_refptr<MockUrlCheckerDelegate> url_checker_delegate_;
  std::unique_ptr<MockThrottleDelegate> throttle_delegate_;
};

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DoesNotDeferOnSafeDocumentUrl) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);

  bool defer = CallWillStartRequest();
  EXPECT_FALSE(defer);
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), 0);

  defer = CallWillProcessResponse();
  EXPECT_FALSE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DeferOnUnsafeDocumentUrl) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/true,
                                /*should_delay_callback=*/false);

  bool defer = CallWillStartRequest();
  // Safe Browsing and URL loader are performed in parallel. Safe Browsing
  // doesn't defer the start of the request.
  EXPECT_FALSE(defer);
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);
  EXPECT_EQ(throttle_delegate_->GetCustomReason(), "SafeBrowsing");

  defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DoesNotDeferOnUnsafeIframeUrl) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/true,
                                /*should_delay_callback=*/false);

  bool defer = CallWillStartRequestWithDestination(
      network::mojom::RequestDestination::kIframe);
  EXPECT_FALSE(defer);
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), 0);

  defer = CallWillProcessResponse();
  EXPECT_FALSE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DeferIfRedirectUrlIsUnsafe) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/true,
                                /*should_delay_callback=*/false);

  CallWillStartRequest();
  CallWillRedirectRequest();
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);

  bool defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DoesNotDeferOnSkippedUrl) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/true,
                                /*should_delay_callback=*/false);
  url_checker_delegate_->EnableSkipRequestCheck();

  CallWillStartRequestWithDestination(
      network::mojom::RequestDestination::kEmpty);

  bool defer = CallWillProcessResponse();
  // The loader is not deferred because the check has skipped.
  EXPECT_FALSE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DoesNotDeferOnKnownSafeUrl) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);

  bool defer = false;
  network::ResourceRequest request;
  request.url = GURL("chrome://new-tab-page");
  throttle_->WillStartRequest(&request, &defer);
  task_environment_.RunUntilIdle();

  CallWillRedirectRequest();

  defer = CallWillProcessResponse();
  // The loader is not deferred because the URL is known to be safe.
  EXPECT_FALSE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DeferOnSlowCheck) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);

  CallWillStartRequest();

  bool defer = CallWillProcessResponse();
  // Deferred because the check has not completed.
  EXPECT_TRUE(defer);
  EXPECT_FALSE(throttle_delegate_->IsResumed());

  url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(throttle_delegate_->IsResumed());
}

TEST_F(SBBrowserUrlLoaderThrottleTest, VerifyDefer_DeferOnSlowRedirectCheck) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);

  CallWillStartRequest();
  CallWillRedirectRequest();

  bool defer = CallWillProcessResponse();
  // Deferred because the check for redirect URL has not completed.
  EXPECT_TRUE(defer);
  EXPECT_FALSE(throttle_delegate_->IsResumed());

  url_checker_->RestartDelayedCallback(/*index=*/1);
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(throttle_delegate_->IsResumed());
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DoesNotResumeOnSlowCheckNotProceed) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/true,
                                /*should_delay_callback=*/true);

  CallWillStartRequest();

  bool defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
  EXPECT_FALSE(throttle_delegate_->IsResumed());

  url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();
  // Resume should not be called because the slow check returns don't proceed.
  EXPECT_FALSE(throttle_delegate_->IsResumed());
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DoesNotDeferRedirectOnSlowCheck) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);

  CallWillStartRequest();

  bool defer = CallWillRedirectRequest();
  // Although the first check has not completed, redirect should not be
  // deferred.
  EXPECT_FALSE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyDefer_DeferRedirectWhenFirstUrlAlreadyBlocked) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/true,
                                /*should_delay_callback=*/false);

  CallWillStartRequest();
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);

  bool defer = CallWillRedirectRequest();
  EXPECT_TRUE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyErrorCodeWhenInterstitialNotShown) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);

  CallWillStartRequest();
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_ABORTED);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_FastCheckFromNetwork) {
  SetUpTest();
  base::HistogramTester histograms;
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);

  CallWillStartRequest();
  task_environment_.FastForwardBy(base::Milliseconds(200));
  CallWillProcessResponse();

  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck",
      base::Milliseconds(0), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromNetwork",
      base::Milliseconds(0), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromCache", 0);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_FastCheckFromCache) {
  SetUpTest();
  base::HistogramTester histograms;
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);

  CallWillStartRequest();
  task_environment_.FastForwardBy(base::Milliseconds(200));
  CallWillProcessResponseFromCache();

  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck",
      base::Milliseconds(0), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromCache",
      base::Milliseconds(0), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromNetwork", 0);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_SlowCheckFromNetwork) {
  SetUpTest();
  base::HistogramTester histograms;
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);

  CallWillStartRequest();
  CallWillProcessResponse();
  task_environment_.FastForwardBy(base::Milliseconds(200));
  url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck",
      base::Milliseconds(200), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromNetwork",
      base::Milliseconds(200), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromCache", 0);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_SlowCheckFromCache) {
  SetUpTest();
  base::HistogramTester histograms;
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/true);

  CallWillStartRequest();
  CallWillProcessResponseFromCache();
  task_environment_.FastForwardBy(base::Milliseconds(200));
  url_checker_->RestartDelayedCallback(/*index=*/0);
  task_environment_.RunUntilIdle();

  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck",
      base::Milliseconds(200), 1);
  histograms.ExpectUniqueTimeSample(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromCache",
      base::Milliseconds(200), 1);
  histograms.ExpectTotalCount(
      "SafeBrowsing.BrowserThrottle.TotalDelay2.FromNetwork", 0);
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_HashPrefixDatabaseCheck) {
  RunTotalDelayHistogramsUrlCheckTypeTest(
      SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashDatabaseCheck,
      /*url_real_time_lookup_enabled=*/false,
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixDatabaseCheck");
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_HashPrefixRealTimeCheck) {
  RunTotalDelayHistogramsUrlCheckTypeTest(
      SafeBrowsingUrlCheckerImpl::PerformedCheck::kHashRealTimeCheck,
      /*url_real_time_lookup_enabled=*/false,
      "SafeBrowsing.BrowserThrottle.TotalDelay2.HashPrefixRealTimeCheck");
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_SkippedCheck) {
  RunTotalDelayHistogramsUrlCheckTypeTest(
      SafeBrowsingUrlCheckerImpl::PerformedCheck::kCheckSkipped,
      /*url_real_time_lookup_enabled=*/false,
      "SafeBrowsing.BrowserThrottle.TotalDelay2.SkippedCheck");
}

TEST_F(SBBrowserUrlLoaderThrottleTest,
       VerifyTotalDelayHistograms_UrlRealTimeCheck) {
  RunTotalDelayHistogramsUrlCheckTypeTest(
      SafeBrowsingUrlCheckerImpl::PerformedCheck::kUrlRealTimeCheck,
      /*url_real_time_lookup_enabled=*/true,
      "SafeBrowsing.BrowserThrottle.TotalDelay2.MockFullUrlLookup");
}

class SBBrowserUrlLoaderThrottleDisableSkipSubresourcesTest
    : public SBBrowserUrlLoaderThrottleTest {
 public:
  SBBrowserUrlLoaderThrottleDisableSkipSubresourcesTest() {
    feature_list_.InitAndDisableFeature(kSafeBrowsingSkipSubresources);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(SBBrowserUrlLoaderThrottleDisableSkipSubresourcesTest,
       VerifyDefer_DoesNotDeferOnSafeDocumentUrl) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/true,
                                /*should_show_interstitial=*/false,
                                /*should_delay_callback=*/false);

  bool defer = CallWillStartRequest();
  EXPECT_FALSE(defer);
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), 0);

  defer = CallWillProcessResponse();
  EXPECT_FALSE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleDisableSkipSubresourcesTest,
       VerifyDefer_DefersOnUnsafeDocumentUrl) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/true,
                                /*should_delay_callback=*/false);

  bool defer = CallWillStartRequest();
  // Safe Browsing and URL loader are performed in parallel. Safe Browsing
  // doesn't defer the start of the request.
  EXPECT_FALSE(defer);
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);
  EXPECT_EQ(throttle_delegate_->GetCustomReason(), "SafeBrowsing");

  defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
}

TEST_F(SBBrowserUrlLoaderThrottleDisableSkipSubresourcesTest,
       VerifyDefer_DefersOnUnsafeIframeUrl) {
  SetUpTest();
  url_checker_->AddCallbackInfo(/*should_proceed=*/false,
                                /*should_show_interstitial=*/true,
                                /*should_delay_callback=*/false);

  bool defer = CallWillStartRequestWithDestination(
      network::mojom::RequestDestination::kIframe);
  // Safe Browsing and URL loader are performed in parallel. Safe Browsing
  // doesn't defer the start of the request.
  EXPECT_FALSE(defer);
  EXPECT_EQ(throttle_delegate_->GetErrorCode(), net::ERR_BLOCKED_BY_CLIENT);
  EXPECT_EQ(throttle_delegate_->GetCustomReason(), "SafeBrowsing");

  defer = CallWillProcessResponse();
  EXPECT_TRUE(defer);
}

}  // namespace safe_browsing

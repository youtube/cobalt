// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/browser/hashprefix_realtime/hash_realtime_service.h"
#include "components/safe_browsing/core/browser/ping_manager.h"
#include "components/safe_browsing/core/browser/realtime/policy_engine.h"
#include "components/safe_browsing/core/browser/realtime/url_lookup_service_base.h"
#include "components/safe_browsing/core/browser/safe_browsing_lookup_mechanism_experimenter.h"
#include "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#include "components/safe_browsing/core/browser/url_checker_delegate.h"
#include "components/safe_browsing/core/browser/utils/scheme_logger.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "components/safe_browsing/core/common/utils.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/base/load_flags.h"
#include "net/log/net_log_event_type.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

constexpr char kFullURLLookup[] = "FullUrlLookup";

constexpr char kFromCacheUmaSuffix[] = ".FromCache";
constexpr char kFromNetworkUmaSuffix[] = ".FromNetwork";

void LogTotalDelay2Metrics(const std::string& url_check_type,
                           bool did_check_allowlist,
                           base::TimeDelta total_delay) {
  base::UmaHistogramTimes(
      base::StrCat(
          {"SafeBrowsing.BrowserThrottle.TotalDelay2", url_check_type}),
      total_delay);
  if (url_check_type == base::StrCat({".Enterprise", kFullURLLookup})) {
    base::UmaHistogramTimes(
        base::StrCat(
            {"SafeBrowsing.BrowserThrottle.TotalDelay2.EnterpriseFullUrlLookup",
             did_check_allowlist ? ".AllowlistChecked" : ".AllowlistBypassed"}),
        total_delay);
  }
}

void LogTotalDelay2MetricsWithResponseType(bool is_response_from_cache,
                                           base::TimeDelta total_delay) {
  base::UmaHistogramTimes(
      base::StrCat({"SafeBrowsing.BrowserThrottle.TotalDelay2",
                    is_response_from_cache ? kFromCacheUmaSuffix
                                           : kFromNetworkUmaSuffix}),
      total_delay);
}

// Returns true if the URL is known to be safe. We also require that this URL
// never redirects to a potentially unsafe URL, because the redirected URLs are
// also skipped if this function returns true.
bool KnownSafeUrl(const GURL& url) {
  return url.SchemeIs(content::kChromeUIScheme) &&
         !safe_browsing::IsSafeBrowsingWebUIUrl(url);
}

}  // namespace

namespace safe_browsing {

BrowserURLLoaderThrottle::CheckerOnSB::CheckerOnSB(
    GetDelegateCallback delegate_getter,
    int frame_tree_node_id,
    base::RepeatingCallback<content::WebContents*()> web_contents_getter,
    base::WeakPtr<BrowserURLLoaderThrottle> throttle,
    bool real_time_lookup_enabled,
    bool can_rt_check_subresource_url,
    bool can_check_db,
    bool can_check_high_confidence_allowlist,
    std::string url_lookup_service_metric_suffix,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
    base::WeakPtr<HashRealTimeService> hash_realtime_service,
    base::WeakPtr<PingManager> ping_manager,
    bool is_mechanism_experiment_allowed)
    : delegate_getter_(std::move(delegate_getter)),
      frame_tree_node_id_(frame_tree_node_id),
      web_contents_getter_(web_contents_getter),
      throttle_(std::move(throttle)),
      real_time_lookup_enabled_(real_time_lookup_enabled),
      can_rt_check_subresource_url_(can_rt_check_subresource_url),
      can_check_db_(can_check_db),
      can_check_high_confidence_allowlist_(can_check_high_confidence_allowlist),
      url_lookup_service_metric_suffix_(url_lookup_service_metric_suffix),
      url_lookup_service_(url_lookup_service),
      hash_realtime_service_(hash_realtime_service),
      ping_manager_(ping_manager),
      is_mechanism_experiment_allowed_(is_mechanism_experiment_allowed),
      creation_time_(base::TimeTicks::Now()) {
  content::WebContents* contents = web_contents_getter_.Run();
  if (!!contents) {
    last_committed_url_ = contents->GetLastCommittedURL();
  }
}

BrowserURLLoaderThrottle::CheckerOnSB::~CheckerOnSB() {
  base::UmaHistogramMediumTimes(
      "SafeBrowsing.BrowserThrottle.CheckerOnIOLifetime",
      base::TimeTicks::Now() - creation_time_);
  if (mechanism_experimenter_) {
    mechanism_experimenter_->OnBrowserUrlLoaderThrottleCheckerOnSBDestructed();
  }
}

void BrowserURLLoaderThrottle::CheckerOnSB::Start(
    const net::HttpRequestHeaders& headers,
    int load_flags,
    network::mojom::RequestDestination request_destination,
    bool has_user_gesture,
    bool originated_from_service_worker,
    const GURL& url,
    const std::string& method) {
  DCHECK_CURRENTLY_ON(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::BrowserThread::UI
          : content::BrowserThread::IO);
  scoped_refptr<UrlCheckerDelegate> url_checker_delegate =
      std::move(delegate_getter_).Run();
  skip_checks_ =
      !url_checker_delegate ||
      url_checker_delegate->ShouldSkipRequestCheck(
          url, frame_tree_node_id_,
          content::ChildProcessHost::kInvalidUniqueID /* render_process_id */,
          MSG_ROUTING_NONE /* render_frame_id */,
          originated_from_service_worker);
  if (skip_checks_) {
    if (base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)) {
      throttle_->SkipChecks();
    } else {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&BrowserURLLoaderThrottle::SkipChecks, throttle_));
    }
    return;
  }

  if (is_mechanism_experiment_allowed_ &&
      request_destination == network::mojom::RequestDestination::kDocument) {
    mechanism_experimenter_ =
        base::MakeRefCounted<SafeBrowsingLookupMechanismExperimenter>(
            /*is_prefetch=*/load_flags & net::LOAD_PREFETCH,
            /*ping_manager_on_ui=*/ping_manager_,
            /*ui_task_runner=*/content::GetUIThreadTaskRunner({}));
  }
  if (url_checker_for_testing_) {
    url_checker_ = std::move(url_checker_for_testing_);
  } else {
    url_checker_ = std::make_unique<SafeBrowsingUrlCheckerImpl>(
        headers, load_flags, request_destination, has_user_gesture,
        url_checker_delegate, web_contents_getter_,
        content::ChildProcessHost::kInvalidUniqueID, MSG_ROUTING_NONE,
        frame_tree_node_id_, real_time_lookup_enabled_,
        can_rt_check_subresource_url_, can_check_db_,
        can_check_high_confidence_allowlist_, url_lookup_service_metric_suffix_,
        last_committed_url_, content::GetUIThreadTaskRunner({}),
        url_lookup_service_, WebUIInfoSingleton::GetInstance(),
        hash_realtime_service_, mechanism_experimenter_,
        is_mechanism_experiment_allowed_);
  }

  CheckUrl(url, method);
}

void BrowserURLLoaderThrottle::CheckerOnSB::CheckUrl(
    const GURL& url,
    const std::string& method) {
  DCHECK_CURRENTLY_ON(
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::BrowserThread::UI
          : content::BrowserThread::IO);
  if (skip_checks_) {
    if (base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)) {
      throttle_->SkipChecks();
    } else {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&BrowserURLLoaderThrottle::SkipChecks, throttle_));
    }
    return;
  }

  DCHECK(url_checker_);
  url_checker_->CheckUrl(
      url, method,
      base::BindOnce(&BrowserURLLoaderThrottle::CheckerOnSB::OnCheckUrlResult,
                     base::Unretained(this)));
}

void BrowserURLLoaderThrottle::CheckerOnSB::LogWillProcessResponseTime(
    base::TimeTicks reached_time) {
  if (mechanism_experimenter_) {
    mechanism_experimenter_->OnWillProcessResponseReached(reached_time);
  }
}

void BrowserURLLoaderThrottle::CheckerOnSB::SetUrlCheckerForTesting(
    std::unique_ptr<SafeBrowsingUrlCheckerImpl> checker) {
  url_checker_for_testing_ = std::move(checker);
}

void BrowserURLLoaderThrottle::CheckerOnSB::OnCheckUrlResult(
    NativeUrlCheckNotifier* slow_check_notifier,
    bool proceed,
    bool showed_interstitial,
    bool did_perform_real_time_check,
    bool did_check_allowlist) {
  if (!slow_check_notifier) {
    OnCompleteCheck(false /* slow_check */, proceed, showed_interstitial,
                    did_perform_real_time_check, did_check_allowlist);
    return;
  }

  if (base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)) {
    throttle_->NotifySlowCheck();
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&BrowserURLLoaderThrottle::NotifySlowCheck, throttle_));
  }

  // In this case |proceed| and |showed_interstitial| should be ignored. The
  // result will be returned by calling |*slow_check_notifier| callback.
  *slow_check_notifier =
      base::BindOnce(&BrowserURLLoaderThrottle::CheckerOnSB::OnCompleteCheck,
                     base::Unretained(this), true /* slow_check */);
}

void BrowserURLLoaderThrottle::CheckerOnSB::OnCompleteCheck(
    bool slow_check,
    bool proceed,
    bool showed_interstitial,
    bool did_perform_real_time_check,
    bool did_check_allowlist) {
  if (base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)) {
    throttle_->OnCompleteCheck(slow_check, proceed, showed_interstitial,
                               did_perform_real_time_check,
                               did_check_allowlist);
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&BrowserURLLoaderThrottle::OnCompleteCheck, throttle_,
                       slow_check, proceed, showed_interstitial,
                       did_perform_real_time_check, did_check_allowlist));
  }
}

// static
std::unique_ptr<BrowserURLLoaderThrottle> BrowserURLLoaderThrottle::Create(
    GetDelegateCallback delegate_getter,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter,
    int frame_tree_node_id,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
    base::WeakPtr<HashRealTimeService> hash_realtime_service,
    base::WeakPtr<PingManager> ping_manager) {
  return base::WrapUnique<BrowserURLLoaderThrottle>(
      new BrowserURLLoaderThrottle(
          std::move(delegate_getter), web_contents_getter, frame_tree_node_id,
          url_lookup_service, hash_realtime_service, ping_manager));
}

BrowserURLLoaderThrottle::BrowserURLLoaderThrottle(
    GetDelegateCallback delegate_getter,
    const base::RepeatingCallback<content::WebContents*()>& web_contents_getter,
    int frame_tree_node_id,
    base::WeakPtr<RealTimeUrlLookupServiceBase> url_lookup_service,
    base::WeakPtr<HashRealTimeService> hash_realtime_service,
    base::WeakPtr<PingManager> ping_manager) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Decide whether to do real time URL lookups or not.
  real_time_lookup_enabled_ =
      url_lookup_service ? url_lookup_service->CanPerformFullURLLookup()
                         : false;

  bool can_rt_check_subresource_url =
      url_lookup_service && url_lookup_service->CanCheckSubresourceURL();

// This BUILDFLAG check is not strictly necessary because the feature should
// only be enabled for Desktop. This check is included only as a precaution and
// for clarity.
#if BUILDFLAG(FULL_SAFE_BROWSING)
  bool is_mechanism_experiment_allowed =
      hash_realtime_service &&
      hash_realtime_service->IsEnhancedProtectionEnabled() &&
      base::FeatureList::IsEnabled(kSafeBrowsingLookupMechanismExperiment);
#else
  bool is_mechanism_experiment_allowed = false;
#endif

  // Decide whether safe browsing database can be checked.
  // If url_lookup_service is null, safe browsing database should be checked by
  // default.
  bool can_check_db =
      url_lookup_service ? url_lookup_service->CanCheckSafeBrowsingDb() : true;
  bool can_check_high_confidence_allowlist =
      url_lookup_service
          ? url_lookup_service->CanCheckSafeBrowsingHighConfidenceAllowlist()
          : true;

  url_lookup_service_metric_suffix_ =
      real_time_lookup_enabled_ ? url_lookup_service->GetMetricSuffix()
                                : kNoRealTimeURLLookupService;

  sb_checker_ = std::make_unique<CheckerOnSB>(
      std::move(delegate_getter), frame_tree_node_id, web_contents_getter,
      weak_factory_.GetWeakPtr(), real_time_lookup_enabled_,
      can_rt_check_subresource_url, can_check_db,
      can_check_high_confidence_allowlist, url_lookup_service_metric_suffix_,
      url_lookup_service, hash_realtime_service, ping_manager,
      is_mechanism_experiment_allowed);
}

BrowserURLLoaderThrottle::~BrowserURLLoaderThrottle() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (deferred_) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "Deferred",
                                    TRACE_ID_LOCAL(this));
  }

  DeleteCheckerOnSB();
}

void BrowserURLLoaderThrottle::WillStartRequest(
    network::ResourceRequest* request,
    bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(0u, pending_checks_);
  DCHECK(!blocked_);
  base::UmaHistogramBoolean(
      "SafeBrowsing.BrowserThrottle.WillStartRequestAfterWillProcessResponse",
      will_process_response_count_ > 0);
  base::UmaHistogramEnumeration(
      "SafeBrowsing.BrowserThrottle.RequestDestination", request->destination);

  if (KnownSafeUrl(request->url)) {
    skip_checks_ = true;
    return;
  }

  original_url_ = request->url;
  pending_checks_++;
  start_request_time_ = base::TimeTicks::Now();
  is_start_request_called_ = true;
  request_destination_ = request->destination;
  if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
    sb_checker_->Start(request->headers, request->load_flags,
                       request->destination, request->has_user_gesture,
                       request->originated_from_service_worker, request->url,
                       request->method);
  } else {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&BrowserURLLoaderThrottle::CheckerOnSB::Start,
                                  sb_checker_->AsWeakPtr(), request->headers,
                                  request->load_flags, request->destination,
                                  request->has_user_gesture,
                                  request->originated_from_service_worker,
                                  request->url, request->method));
  }
}

void BrowserURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& /* response_head */,
    bool* defer,
    std::vector<std::string>* /* to_be_removed_headers */,
    net::HttpRequestHeaders* /* modified_headers */,
    net::HttpRequestHeaders* /* modified_cors_exempt_headers */) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::UmaHistogramBoolean(
      "SafeBrowsing.BrowserThrottle."
      "WillRedirectRequestAfterWillProcessResponse",
      will_process_response_count_ > 0);

  // TODO(crbug.com/1410939): Below histograms are for debugging. Remove them
  // afterwards.
  safe_browsing::scheme_logger::LogScheme(
      original_url_,
      "SafeBrowsing.BrowserThrottle.RedirectedOriginalUrlScheme");
  if (original_url_.SchemeIs("chrome-extension")) {
    safe_browsing::scheme_logger::LogScheme(
        redirect_info->new_url,
        "SafeBrowsing.BrowserThrottle.RedirectedExtensionUrlScheme");
  }

  if (blocked_) {
    // OnCheckUrlResult() has set |blocked_| to true and called
    // |delegate_->CancelWithError|, but this method is called before the
    // request is actually cancelled. In that case, simply defer the request.
    *defer = true;
    return;
  }

  if (skip_checks_) {
    return;
  }

  pending_checks_++;
  if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
    sb_checker_->CheckUrl(redirect_info->new_url, redirect_info->new_method);
  } else {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&BrowserURLLoaderThrottle::CheckerOnSB::CheckUrl,
                       sb_checker_->AsWeakPtr(), redirect_info->new_url,
                       redirect_info->new_method));
  }
}

void BrowserURLLoaderThrottle::WillProcessResponse(
    const GURL& response_url,
    network::mojom::URLResponseHead* response_head,
    bool* defer) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  will_process_response_count_++;
  base::UmaHistogramCounts100(
      "SafeBrowsing.BrowserThrottle.WillProcessResponseCount",
      will_process_response_count_);

  if (sb_checker_) {
    if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
      sb_checker_->LogWillProcessResponseTime(base::TimeTicks::Now());
    } else {
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&BrowserURLLoaderThrottle::CheckerOnSB::
                             LogWillProcessResponseTime,
                         sb_checker_->AsWeakPtr(), base::TimeTicks::Now()));
    }
  }

  if (blocked_) {
    // OnCheckUrlResult() has set |blocked_| to true and called
    // |delegate_->CancelWithError|, but this method is called before the
    // request is actually cancelled. In that case, simply defer the request.
    *defer = true;
    return;
  }

  bool check_completed = (pending_checks_ == 0);
  base::UmaHistogramBoolean(
      "SafeBrowsing.BrowserThrottle.IsCheckCompletedOnProcessResponse",
      check_completed);
  is_response_from_cache_ =
      response_head->was_fetched_via_cache && !response_head->network_accessed;
  if (is_start_request_called_) {
    base::TimeDelta interval = base::TimeTicks::Now() - start_request_time_;
    base::UmaHistogramTimes(
        "SafeBrowsing.BrowserThrottle.IntervalBetweenStartAndProcess",
        interval);
    base::UmaHistogramTimes(
        base::StrCat(
            {"SafeBrowsing.BrowserThrottle.IntervalBetweenStartAndProcess",
             is_response_from_cache_ ? kFromCacheUmaSuffix
                                     : kFromNetworkUmaSuffix}),
        interval);
    // TODO(crbug.com/1410939): Below histograms are for debugging. Remove them
    // afterwards.
    if (!is_response_from_cache_ && interval <= base::Milliseconds(2)) {
      base::UmaHistogramEnumeration(
          "SafeBrowsing.BrowserThrottle.FastRequestFromNetwork."
          "RequestDestination",
          request_destination_);
      safe_browsing::scheme_logger::LogScheme(
          original_url_,
          "SafeBrowsing.BrowserThrottle.FastRequestFromNetwork.UrlScheme");
    }

    if (check_completed) {
      LogTotalDelay2MetricsWithResponseType(is_response_from_cache_,
                                            base::TimeDelta());
    }
    is_start_request_called_ = false;
  }

  if (check_completed) {
    return;
  }

  DCHECK(!deferred_);
  deferred_ = true;
  defer_start_time_ = base::TimeTicks::Now();
  *defer = true;
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("safe_browsing", "Deferred",
                                    TRACE_ID_LOCAL(this), "original_url",
                                    original_url_.spec());
}

const char* BrowserURLLoaderThrottle::NameForLoggingWillProcessResponse() {
  return "SafeBrowsingBrowserThrottle";
}

BrowserURLLoaderThrottle::CheckerOnSB*
BrowserURLLoaderThrottle::GetSBCheckerForTesting() {
  return sb_checker_.get();
}

void BrowserURLLoaderThrottle::OnCompleteCheck(bool slow_check,
                                               bool proceed,
                                               bool showed_interstitial,
                                               bool did_perform_real_time_check,
                                               bool did_check_allowlist) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!blocked_);
  DCHECK(real_time_lookup_enabled_ || !did_perform_real_time_check);

  DCHECK_LT(0u, pending_checks_);
  pending_checks_--;

  if (slow_check) {
    DCHECK_LT(0u, pending_slow_checks_);
    pending_slow_checks_--;
  }

  // If the resource load is going to finish (either being cancelled or
  // resumed), record the total delay.
  if (!proceed || pending_checks_ == 0) {
    // If the resource load is currently deferred, there is a delay.
    if (deferred_) {
      total_delay_ = base::TimeTicks::Now() - defer_start_time_;
      LogTotalDelay2MetricsWithResponseType(is_response_from_cache_,
                                            total_delay_);
    }
    std::string url_check_type =
        (did_perform_real_time_check)
            ? base::StrCat({url_lookup_service_metric_suffix_, kFullURLLookup})
            : ".HashBasedCheck";
    LogTotalDelay2Metrics(url_check_type, did_check_allowlist, total_delay_);
  }

  if (proceed) {
    if (pending_slow_checks_ == 0 && slow_check) {
      delegate_->ResumeReadingBodyFromNet();
    }

    if (pending_checks_ == 0 && deferred_) {
      deferred_ = false;
      TRACE_EVENT_NESTABLE_ASYNC_END0("safe_browsing", "Deferred",
                                      TRACE_ID_LOCAL(this));
      base::UmaHistogramTimes("SafeBrowsing.BrowserThrottle.TotalDelay",
                              total_delay_);
      delegate_->Resume();
    }
  } else {
    blocked_ = true;

    DeleteCheckerOnSB();
    pending_checks_ = 0;
    pending_slow_checks_ = 0;
    // If we didn't show an interstitial, we cancel with ERR_ABORTED to not show
    // an error page either.
    delegate_->CancelWithError(
        showed_interstitial ? kNetErrorCodeForSafeBrowsing : net::ERR_ABORTED,
        kCustomCancelReasonForURLLoader);
  }
}

void BrowserURLLoaderThrottle::SkipChecks() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Future checks for redirects will be skipped.
  skip_checks_ = true;

  pending_checks_--;
  if (pending_checks_ == 0 && deferred_) {
    delegate_->Resume();
  }
}

void BrowserURLLoaderThrottle::NotifySlowCheck() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  pending_slow_checks_++;

  // Pending slow checks indicate that the resource may be unsafe. In that case,
  // pause reading response body from network to minimize the chance of
  // processing unsafe contents (e.g., writing unsafe contents into cache),
  // until we get the results. According to the results, we may resume reading
  // or cancel the resource load.
  // For real time Safe Browsing checks, we continue reading the response body
  // but, similar to hash-based checks, do not process it until we know it is
  // SAFE.
  if (pending_slow_checks_ == 1) {
    delegate_->PauseReadingBodyFromNet();
  }
}

void BrowserURLLoaderThrottle::DeleteCheckerOnSB() {
  if (base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)) {
    sb_checker_.reset();
  } else {
    content::GetIOThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                   std::move(sb_checker_));
  }
}

}  // namespace safe_browsing

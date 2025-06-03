// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/heuristics/opener_heuristic_tab_helper.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/rand_util.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/dips/dips_bounce_detector.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_metrics.h"
#include "chrome/browser/tpcd/heuristics/opener_heuristic_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

using content::NavigationHandle;
using content::RenderFrameHost;
using content::WebContents;
using tpcd::experiment::EnableForIframeTypes;

namespace {

// We don't need to protect this with a lock since it's only set while
// single-threaded in tests.
base::Clock* g_clock = nullptr;

base::Clock* GetClock() {
  return g_clock ? g_clock : base::DefaultClock::GetInstance();
}

}  // namespace

OpenerHeuristicTabHelper::OpenerHeuristicTabHelper(WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<OpenerHeuristicTabHelper>(*web_contents) {}

OpenerHeuristicTabHelper::~OpenerHeuristicTabHelper() = default;

/* static */
base::Clock* OpenerHeuristicTabHelper::SetClockForTesting(base::Clock* clock) {
  return std::exchange(g_clock, clock);
}

void OpenerHeuristicTabHelper::InitPopup(
    const GURL& popup_url,
    base::WeakPtr<OpenerHeuristicTabHelper> opener) {
  popup_observer_ =
      std::make_unique<PopupObserver>(web_contents(), popup_url, opener);

  DIPSService* dips = DIPSService::Get(web_contents()->GetBrowserContext());
  if (!dips) {
    // If DIPS is disabled, we can't look up past interaction.
    // TODO(rtarpine): consider falling back to SiteEngagementService.
    return;
  }

  dips->storage()
      ->AsyncCall(&DIPSStorage::Read)
      .WithArgs(popup_url)
      .Then(base::BindOnce(&OpenerHeuristicTabHelper::GotPopupDipsState,
                           weak_factory_.GetWeakPtr()));
}

void OpenerHeuristicTabHelper::GotPopupDipsState(const DIPSState& state) {
  if (!state.user_interaction_times().has_value()) {
    // No previous interaction.
    return;
  }

  popup_observer_->SetPastInteractionTime(
      state.user_interaction_times().value().second);
}

void OpenerHeuristicTabHelper::PrimaryPageChanged(content::Page& page) {
  page_id_++;
}

void OpenerHeuristicTabHelper::DidOpenRequestedURL(
    WebContents* new_contents,
    RenderFrameHost* source_render_frame_host,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    bool started_from_context_menu,
    bool renderer_initiated) {
  if (source_render_frame_host->GetMainFrame() !=
      web_contents()->GetPrimaryMainFrame()) {
    // Not sure exactly when this happens, but it seems to involve devtools.
    // Cf. crbug.com/1448789
    return;
  }

  if (!PassesIframeInitiatorCheck(source_render_frame_host)) {
    return;
  }

  if (!new_contents->HasOpener()) {
    // Ignore if popup doesn't have opener access.
    return;
  }

  // Create an OpenerHeuristicTabHelper for the popup.
  //
  // Note: TabHelpers::AttachTabHelpers() creates OpenerHeuristicTabHelper, but
  // on Android that can happen after DidOpenRequestedURL() is called (on other
  // platforms it seems to happen first). So create it now if it doesn't already
  // exist.
  OpenerHeuristicTabHelper::CreateForWebContents(new_contents);
  OpenerHeuristicTabHelper::FromWebContents(new_contents)
      ->InitPopup(url, weak_factory_.GetWeakPtr());
}

bool OpenerHeuristicTabHelper::PassesIframeInitiatorCheck(
    content::RenderFrameHost* source_render_frame_host) {
  if (source_render_frame_host->IsInPrimaryMainFrame()) {
    return true;
  }

  switch (tpcd::experiment::kTpcdPopupHeuristicEnableForIframeInitiator.Get()) {
    case EnableForIframeTypes::kNone:
      return false;
    case EnableForIframeTypes::kAll:
      return true;
    case EnableForIframeTypes::kFirstParty: {
      // Check that the frame tree consists of only first-party iframes.
      std::string main_frame_site = GetSiteForDIPS(
          source_render_frame_host->GetMainFrame()->GetLastCommittedURL());
      RenderFrameHost* rfh_itr = source_render_frame_host;
      while (rfh_itr->GetParent() != nullptr) {
        if (GetSiteForDIPS(rfh_itr->GetLastCommittedURL()) != main_frame_site) {
          return false;
        }
        rfh_itr = rfh_itr->GetParent();
      }
      return true;
    }
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OpenerHeuristicTabHelper);

OpenerHeuristicTabHelper::PopupObserver::PopupObserver(
    WebContents* web_contents,
    const GURL& initial_url,
    base::WeakPtr<OpenerHeuristicTabHelper> opener)
    : content::WebContentsObserver(web_contents),
      popup_id_(static_cast<int32_t>(base::RandUint64())),
      initial_url_(initial_url),
      opener_(opener),
      opener_page_id_(opener->page_id()),
      opener_source_id_(
          opener->web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId()),
      opener_url_(
          opener->web_contents()->GetPrimaryMainFrame()->GetLastCommittedURL()),
      cookie_settings_(CookieSettingsFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()))) {}

OpenerHeuristicTabHelper::PopupObserver::~PopupObserver() = default;

void OpenerHeuristicTabHelper::PopupObserver::SetPastInteractionTime(
    base::Time time) {
  CHECK(!time_since_interaction_.has_value())
      << "SetPastInteractionTime() called more than once";
  // Technically we should use the time when the pop-up first opened. But since
  // we only report this metric at hourly granularity, it shouldn't matter.
  time_since_interaction_ = GetClock()->Now() - time;

  // TODO(rtarpine): consider ignoring interactions that are too old. (This
  // shouldn't happen since DIPS already discards old timestamps.)

  EmitPastInteractionIfReady();
}

void OpenerHeuristicTabHelper::PopupObserver::EmitPastInteractionIfReady() {
  if (!time_since_interaction_.has_value() || !initial_source_id_.has_value()) {
    // Not enough information to emit event yet.
    return;
  }

  MaybeCreateOpenerHeuristicGrant(
      initial_url_,
      tpcd::experiment::kTpcdWritePopupPastInteractionHeuristicsGrants.Get());

  auto has_iframe = GetOpenerHasSameSiteIframe(initial_url_);
  ukm::builders::OpenerHeuristic_PopupPastInteraction(
      initial_source_id_.value())
      .SetHoursSinceLastInteraction(Bucketize3PCDHeuristicTimeDelta(
          time_since_interaction_.value(), base::Days(30),
          base::BindRepeating(&base::TimeDelta::InHours)
              .Then(base::BindRepeating([](int64_t t) { return t; }))))
      .SetOpenerHasSameSiteIframe(static_cast<int64_t>(has_iframe))
      .SetPopupId(popup_id_)
      .Record(ukm::UkmRecorder::Get());

  EmitTopLevel(initial_url_, has_iframe, /*is_current_interaction=*/false);
}

void OpenerHeuristicTabHelper::PopupObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  url_index_ += navigation_handle->GetRedirectChain().size();

  // This is only called on the first committed navigation in the new popup.
  // Only get the source id, time, and ad-tagged status for the first commit.
  // Ignore the rest.
  if (!initial_source_id_.has_value()) {
    commit_time_ = GetClock()->Now();

    if (navigation_handle->GetRedirectChain().size() > 1) {
      // Get a source id for the URL the popup was originally opened with,
      // even though the user was redirected elsewhere.
      initial_source_id_ = GetInitialRedirectSourceId(navigation_handle);
    } else {
      // No redirect happened, get the source id for the committed page.
      initial_source_id_ = navigation_handle->GetNextPageUkmSourceId();
    }

    is_last_navigation_ad_tagged_ =
        navigation_handle->GetNavigationInitiatorActivationAndAdStatus() ==
        blink::mojom::NavigationInitiatorActivationAndAdStatus::
            kStartedWithTransientActivationFromAd;

    EmitPastInteractionIfReady();
  }
}

void OpenerHeuristicTabHelper::PopupObserver::FrameReceivedUserActivation(
    RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  if (interaction_reported_) {
    // Only report the first interaction.
    return;
  }

  if (!commit_time_.has_value()) {
    // Not sure if this can happen. What happens if the user clicks before the
    // popup loads its initial URL?
    return;
  }

  const GURL& interaction_url = render_frame_host->GetLastCommittedURL();
  MaybeCreateOpenerHeuristicGrant(
      interaction_url,
      tpcd::experiment::kTpcdWritePopupCurrentInteractionHeuristicsGrants
          .Get());

  auto time_since_committed = GetClock()->Now() - *commit_time_;
  auto has_iframe = GetOpenerHasSameSiteIframe(interaction_url);
  ukm::builders::OpenerHeuristic_PopupInteraction(
      render_frame_host->GetPageUkmSourceId())
      .SetSecondsSinceCommitted(Bucketize3PCDHeuristicTimeDelta(
          time_since_committed, base::Minutes(3),
          base::BindRepeating(&base::TimeDelta::InSeconds)))
      .SetUrlIndex(url_index_)
      .SetOpenerHasSameSiteIframe(static_cast<int64_t>(has_iframe))
      .SetPopupId(popup_id_)
      .Record(ukm::UkmRecorder::Get());

  interaction_reported_ = true;

  EmitTopLevel(interaction_url, has_iframe,
               /*is_current_interaction=*/true);
}

void OpenerHeuristicTabHelper::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  if (!render_frame_host->IsInLifecycleState(
          content::RenderFrameHost::LifecycleState::kPrerendering)) {
    OnCookiesAccessed(render_frame_host->GetPageUkmSourceId(), details);
  }
}

void OpenerHeuristicTabHelper::OnCookiesAccessed(
    content::NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  OnCookiesAccessed(navigation_handle->GetNextPageUkmSourceId(), details);
}

void OpenerHeuristicTabHelper::OnCookiesAccessed(
    const ukm::SourceId& source_id,
    const content::CookieAccessDetails& details) {
  DIPSService* dips = DIPSService::Get(web_contents()->GetBrowserContext());
  if (!dips) {
    // If DIPS is disabled, we can't look up past popup events.
    // TODO(rtarpine): consider falling back to SiteEngagementService.
    return;
  }

  // Ignore same-domain cookie access.
  if (details.first_party_url.is_empty() ||
      GetSiteForDIPS(details.first_party_url) == GetSiteForDIPS(details.url)) {
    return;
  }

  dips->storage()
      ->AsyncCall(&DIPSStorage::ReadPopup)
      .WithArgs(GetSiteForDIPS(details.first_party_url),
                GetSiteForDIPS(details.url))
      .Then(base::BindOnce(&OpenerHeuristicTabHelper::EmitPostPopupCookieAccess,
                           weak_factory_.GetWeakPtr(), source_id, details));
}

void OpenerHeuristicTabHelper::EmitPostPopupCookieAccess(
    const ukm::SourceId& source_id,
    const content::CookieAccessDetails& details,
    absl::optional<PopupsStateValue> value) {
  if (!value.has_value()) {
    return;
  }
  int32_t hours_since_opener = Bucketize3PCDHeuristicTimeDelta(
      GetClock()->Now() - value->last_popup_time, base::Days(30),
      base::BindRepeating(&base::TimeDelta::InHours)
          .Then(base::BindRepeating([](int64_t t) { return t; })));

  ukm::builders::OpenerHeuristic_PostPopupCookieAccess(source_id)
      .SetAccessId(value->access_id)
      .SetAccessSucceeded(!details.blocked_by_policy)
      .SetHoursSincePopupOpened(hours_since_opener)
      .Record(ukm::UkmRecorder::Get());
}

void OpenerHeuristicTabHelper::PopupObserver::EmitTopLevel(
    const GURL& popup_url,
    OptionalBool has_iframe,
    bool is_current_interaction) {
  uint64_t access_id = base::RandUint64();

  if (DIPSService* dips =
          DIPSService::Get(web_contents()->GetBrowserContext())) {
    dips->storage()
        ->AsyncCall(&DIPSStorage::WritePopup)
        .WithArgs(GetSiteForDIPS(opener_url_), GetSiteForDIPS(popup_url),
                  access_id,
                  /*popup_time=*/GetClock()->Now(), is_current_interaction)
        .Then(base::BindOnce([](bool succeeded) { DCHECK(succeeded); }));
  }

  if (toplevel_reported_) {
    return;
  }

  ukm::builders::OpenerHeuristic_TopLevel(opener_source_id_)
      .SetAccessId(access_id)
      .SetHasSameSiteIframe(static_cast<int64_t>(has_iframe))
      .SetPopupProvider(static_cast<int64_t>(GetPopupProvider(initial_url_)))
      .SetPopupId(popup_id_)
      .SetIsAdTaggedPopupClick(is_last_navigation_ad_tagged_)
      .Record(ukm::UkmRecorder::Get());

  toplevel_reported_ = true;
}

void OpenerHeuristicTabHelper::PopupObserver::MaybeCreateOpenerHeuristicGrant(
    const GURL& url,
    base::TimeDelta grant_duration) {
  if (!grant_duration.is_positive()) {
    return;
  }

  if (is_last_navigation_ad_tagged_ &&
      tpcd::experiment::kTpcdPopupHeuristicDisableForAdTaggedPopups.Get()) {
    return;
  }

  cookie_settings_->SetTemporaryCookieGrantForHeuristic(url, opener_url_,
                                                        grant_duration);
}

OptionalBool
OpenerHeuristicTabHelper::PopupObserver::GetOpenerHasSameSiteIframe(
    const GURL& popup_url) {
  if (opener_ && opener_->page_id() == opener_page_id_) {
    return ToOptionalBool(
        HasSameSiteIframe(opener_->web_contents(), popup_url));
  }

  return OptionalBool::kUnknown;
}

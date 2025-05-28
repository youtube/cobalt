// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dips/btm_page_visit_observer.h"

#include "content/browser/dips/cookie_access_filter.h"
#include "content/browser/dips/dips_bounce_detector.h"
#include "content/browser/dips/dips_utils.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/dips_redirect_info.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_handle_user_data.h"
#include "content/public/browser/render_frame_host.h"

namespace content {

BtmNavigationInfo::BtmNavigationInfo(NavigationHandle& navigation_handle)
    : was_user_initiated(!navigation_handle.IsRendererInitiated() ||
                         navigation_handle.HasUserGesture()),
      was_renderer_initiated(navigation_handle.IsRendererInitiated()),
      page_transition(navigation_handle.GetPageTransition()) {}
BtmNavigationInfo::BtmNavigationInfo(const BtmNavigationInfo&) = default;
BtmNavigationInfo::BtmNavigationInfo(BtmNavigationInfo&&) = default;
BtmNavigationInfo::~BtmNavigationInfo() = default;

BtmPageVisitObserver::BtmPageVisitObserver(WebContents* web_contents,
                                           VisitCallback callback,
                                           base::Clock* clock)
    : WebContentsObserver(web_contents),
      callback_(callback),
      current_page_{
          .url = web_contents->GetPrimaryMainFrame()->GetLastCommittedURL(),
          .source_id =
              web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId()},
      clock_(CHECK_DEREF(clock)),
      last_page_change_time_(clock_->Now()) {}

BtmPageVisitObserver::~BtmPageVisitObserver() {
  // Flush any visits still pending. We won't be alive any longer to receive
  // late cookie access notifications so this is the best we can do.
  while (!pending_visits_.empty()) {
    ReportVisit();
  }
}

namespace {

inline bool IsWrite(BtmDataAccessType t) {
  return t == BtmDataAccessType::kWrite || t == BtmDataAccessType::kReadWrite;
}

// State associated with a navigation such as cookie accesses reported on its
// NavigationHandle. This state is used to generate the BtmNavigationInfo passed
// to BtmPageVisitObserver's callback.
class NavigationState
    : public content::NavigationHandleUserData<NavigationState> {
 public:
  explicit NavigationState(NavigationHandle&) {}

  void RecordCookieAccess(const GURL& url, CookieOperation op) {
    filter_.AddAccess(url, op);
  }

  // Returns the navigation info paired with the cookie access of the final
  // (i.e. committed) URL of the navigation.
  std::pair<BtmNavigationInfo, BtmDataAccessType> CreateNavigationInfo(
      NavigationHandle& navigation_handle) {
    BtmNavigationInfo navigation(navigation_handle);

    // Populate navigation.server_redirects.
    std::vector<BtmDataAccessType> accesses;
    filter_.Filter(navigation_handle.GetRedirectChain(), &accesses);
    for (size_t i = 1; i < navigation_handle.GetRedirectChain().size(); ++i) {
      navigation.server_redirects.emplace_back(
          navigation_handle.GetRedirectChain()[i - 1],
          dips::GetRedirectSourceId(&navigation_handle, i - 1),
          IsWrite(accesses[i - 1]));
    }

    return {std::move(navigation), accesses.back()};
  }

  NAVIGATION_HANDLE_USER_DATA_KEY_DECL();

 private:
  CookieAccessFilter filter_;
};

NAVIGATION_HANDLE_USER_DATA_KEY_IMPL(NavigationState);

}  // namespace

void BtmPageVisitObserver::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  // Ignore irrelevant navigations.
  if (!IsInPrimaryPage(navigation_handle)) {
    return;
  }

  NavigationState::CreateForNavigationHandle(*navigation_handle);
}

void BtmPageVisitObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // Ignore irrelevant navigations.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  auto* state = NavigationState::GetForNavigationHandle(*navigation_handle);
  if (!state) {
    // We must have started observing this WebContents after the navigation
    // started, so we're only seeing its end. Ignore it because we don't have
    // enough info to report.
    return;
  }

  base::Time now = clock_->Now();
  current_page_.visit_duration = last_page_change_time_.has_value()
                                     ? (now - last_page_change_time_.value())
                                     : base::TimeDelta();
  auto [navigation, final_url_cookie_access] =
      state->CreateNavigationInfo(*navigation_handle);
  // Don't report the visit right away; put it in the pending queue and wait a
  // bit to see if we receive any late cookie notifications.
  pending_visits_.emplace_back(std::move(current_page_), std::move(navigation),
                               navigation_handle->GetURL());
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&BtmPageVisitObserver::ReportVisit,
                     weak_factory_.GetWeakPtr()),
      base::Seconds(1));

  current_page_ = BtmPageVisitInfo{
      .url = navigation_handle->GetURL(),
      .source_id = navigation_handle->GetNextPageUkmSourceId(),
      .had_qualifying_storage_access = IsWrite(final_url_cookie_access)};
  last_page_change_time_ = now;
}

void BtmPageVisitObserver::ReportVisit() {
  CHECK(!pending_visits_.empty());
  VisitTuple& visit = pending_visits_.front();
  callback_.Run(visit.prev_page, visit.navigation, visit.url);
  pending_visits_.pop_front();
}

void BtmPageVisitObserver::OnCookiesAccessed(
    RenderFrameHost* render_frame_host,
    const CookieAccessDetails& details) {
  // Ignore irrelevant cookie accesses.
  if (details.blocked_by_policy ||
      details.type != CookieAccessDetails::Type::kChange ||
      !dips::IsOrWasInPrimaryPage(render_frame_host)) {
    return;
  }

  // Check to see if this is a late report for a redirect.
  //
  // TODO: crbug.com/394059601 - once we have support for unit-testing cookie
  // accesses, add a unit test for this case.
  for (VisitTuple& visit : pending_visits_) {
    for (BtmServerRedirectInfo& redirect : visit.navigation.server_redirects) {
      if (details.url == redirect.url) {
        redirect.did_write_cookies = true;
        return;
      }
    }
  }

  if (render_frame_host->GetMainFrame()->IsInPrimaryMainFrame()) {
    // Cookie access within the current page.
    current_page_.had_qualifying_storage_access = true;
    return;
  }

  // If the cookie was accessed by a subresource request in a now-bfcached
  // page, try to find that page's visit.
  const GURL& page_url =
      render_frame_host->GetMainFrame()->GetLastCommittedURL();
  for (VisitTuple& visit : pending_visits_) {
    if (page_url == visit.prev_page.url) {
      visit.prev_page.had_qualifying_storage_access = true;
      return;
    }
  }
}

void BtmPageVisitObserver::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const CookieAccessDetails& details) {
  // Ignore irrelevant cookie accesses.
  if (details.blocked_by_policy ||
      details.type != CookieAccessDetails::Type::kChange ||
      !IsInPrimaryPage(navigation_handle)) {
    return;
  }

  if (!navigation_handle->IsInMainFrame()) {
    // Subframe navigation
    current_page_.had_qualifying_storage_access = true;
    return;
  }

  auto* state = NavigationState::GetForNavigationHandle(*navigation_handle);
  if (!state) {
    // We must have started observing this WebContents after the navigation
    // started. Just ignore it; we'll handle the next navigation.
    return;
  }

  state->RecordCookieAccess(details.url, details.type);
}

void BtmPageVisitObserver::FrameReceivedUserActivation(
    RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    CHECK(render_frame_host->GetOutermostMainFrameOrEmbedder()
              ->IsInPrimaryMainFrame());
    return;
  }
  current_page_.received_user_activation = true;
}

void BtmPageVisitObserver::WebAuthnAssertionRequestSucceeded(
    RenderFrameHost* render_frame_host) {
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    CHECK(render_frame_host->GetOutermostMainFrameOrEmbedder()
              ->IsInPrimaryMainFrame());
    return;
  }
  current_page_.had_successful_web_authn_assertion = true;
}

}  // namespace content

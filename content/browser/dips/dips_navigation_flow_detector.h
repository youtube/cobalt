// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DIPS_DIPS_NAVIGATION_FLOW_DETECTOR_H_
#define CONTENT_BROWSER_DIPS_DIPS_NAVIGATION_FLOW_DETECTOR_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "content/browser/dips/dips_bounce_detector.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-forward.h"
#include "url/gurl.h"

namespace content {

struct CookieAccessDetails;
class NavigationHandle;
class RenderFrameHost;

namespace dips {

// Should match DIPSDirectNavigationSource in tools/metrics/histograms/enums.xml
enum class DirectNavigationSource {
  kUnknown = 0,
  kOmnibar = 1,
  kBookmark = 2,
};

struct PageVisitInfo {
  PageVisitInfo();
  PageVisitInfo(PageVisitInfo&& other);

  PageVisitInfo& operator=(PageVisitInfo&& other);

  GURL url;
  std::string site;
  ukm::SourceId source_id;
  bool did_page_access_cookies;
  bool did_page_access_storage;
  bool did_page_receive_user_activation;
  bool did_page_have_successful_waa;
  std::optional<bool> was_navigation_to_page_renderer_initiated;
  std::optional<bool> was_navigation_to_page_user_initiated;

  bool WasNavigationToPageClientRedirect() const;
};

struct EntrypointInfo {
  // Used when the entrypoint has a server redirect exit.
  explicit EntrypointInfo(const BtmRedirectInfo& server_redirect_info,
                          const dips::PageVisitInfo& exit_page_info);
  // Used when the entrypoint has a client redirect exit.
  explicit EntrypointInfo(const dips::PageVisitInfo& client_redirector_info);

  const std::string site;
  ukm::SourceId source_id;
  bool had_triggering_storage_access;
  bool was_referral_client_redirect;
};

enum class FlowStatus {
  kInvalidated = 0,
  kOngoing,
  kEnded,
};

class InFlowSuccessorInteractionState {
 public:
  explicit InFlowSuccessorInteractionState(
      dips::EntrypointInfo flow_entrypoint);
  ~InFlowSuccessorInteractionState();

  void RecordTriggeringStorageAccessByEntrypoint();
  void IncrementFlowIndex(size_t increment);
  void RecordSuccessorInteractionAtCurrentFlowIndex();
  bool IsAtSuccessor() const;

  const dips::EntrypointInfo& flow_entrypoint() const {
    return flow_entrypoint_;
  }
  size_t flow_index() const { return flow_index_; }
  const std::vector<size_t>& successor_interaction_indices() const {
    return successor_interaction_indices_;
  }

 private:
  dips::EntrypointInfo flow_entrypoint_;
  size_t flow_index_ = 0;
  std::vector<size_t> successor_interaction_indices_;
};

}  // namespace dips

// Detects possible navigation flows with the aim of discovering how to
// distinguish user-interest navigation flows from navigational tracking.
// Currently only reports UKM to inform how we might identify possible
// navigational tracking by sites that also perform user-interest activity.
class CONTENT_EXPORT BtmNavigationFlowDetector
    : public RedirectChainDetector::Observer,
      public WebContentsObserver,
      public WebContentsUserData<BtmNavigationFlowDetector> {
 public:
  ~BtmNavigationFlowDetector() override;

  void SetClockForTesting(base::Clock* clock) {
    CHECK(clock);
    clock_ = *clock;
  }

 protected:
  explicit BtmNavigationFlowDetector(WebContents* web_contents);

  void MaybeEmitNavFlowNodeUkmForPreviousPage();
  bool CanEmitNavFlowNodeUkmForPreviousPage() const;

  void MaybeEmitSuspectedTrackerFlowUkmForServerRedirectExit(
      const BtmRedirectInfo* exit_info,
      int32_t flow_id);
  bool CanEmitSuspectedTrackerFlowUkmForServerRedirectExit(
      const BtmRedirectInfo* exit_info) const;
  void MaybeEmitSuspectedTrackerFlowUkmForClientRedirectExit(int32_t flow_id);
  bool CanEmitSuspectedTrackerFlowUkmForClientRedirectExit() const;
  bool CanEmitSuspectedTrackerFlowUkm(
      const dips::PageVisitInfo& referrer_page_info,
      const dips::EntrypointInfo& entrypoint_info,
      const dips::PageVisitInfo& exit_page_info) const;

  void MaybeEmitInFlowInteraction(int32_t flow_id);

  void MaybeEmitInFlowSuccessorInteraction();

 private:
  // So WebContentsUserData::CreateForWebContents can call the constructor.
  friend class WebContentsUserData<BtmNavigationFlowDetector>;

  dips::FlowStatus FlowStatusAfterNavigation(
      bool did_most_recent_navigation_start_new_flow) const;
  // Returns whether the entrypoint was set or not.
  bool MaybeInitializeSuccessorInteractionTrackingState();
  void ResetSuccessorInteractionTrackingState();

  const BtmRedirectContext& GetRedirectContext() const;

  // start WebContentsObserver overrides
  // For client-initiated cookie accesses, and late-reported cookie accesses in
  // navigations.
  void OnCookiesAccessed(RenderFrameHost* render_frame_host,
                         const CookieAccessDetails& details) override;
  // For cookie accesses in navigations.
  void OnCookiesAccessed(NavigationHandle* navigation_handle,
                         const CookieAccessDetails& details) override;
  void NotifyStorageAccessed(RenderFrameHost* render_frame_host,
                             blink::mojom::StorageTypeAccessed storage_type,
                             bool blocked) override;
  void FrameReceivedUserActivation(RenderFrameHost* render_frame_host) override;
  void WebAuthnAssertionRequestSucceeded(
      RenderFrameHost* render_frame_host) override;
  void WebContentsDestroyed() override;
  // end WebContentsObserver overrides

  // start RedirectChainDetector::Observer overrides
  void OnNavigationCommitted(NavigationHandle* navigation_handle) override;
  // end RedirectChainDetector::Observer overrides

  std::optional<dips::PageVisitInfo> two_pages_ago_visit_info_;
  std::optional<dips::PageVisitInfo> previous_page_visit_info_;
  std::optional<dips::PageVisitInfo> current_page_visit_info_;

  // The status of a flow for the purposes of InFlowSuccessorInteraction, after
  // the most recent primary page change.
  dips::FlowStatus flow_status_ = dips::FlowStatus::kInvalidated;
  // Data needed for emitting DIPS.TrustIndicator.InFlowSuccessorInteraction.
  // Set only when there's an ongoing flow that's possibly valid (we can't know
  // for sure until it ends or is invalidated).
  std::optional<dips::InFlowSuccessorInteractionState>
      successor_interaction_tracking_state_;

  // Tracks a navigational cookie access notification that is received before
  // the navigation finishes.
  std::optional<GURL> navigation_cookie_access_url_;

  base::Time last_page_change_time_;
  long bucketized_previous_page_visit_duration_;

  base::ScopedObservation<RedirectChainDetector,
                          RedirectChainDetector::Observer>
      redirect_chain_observation_{this};

  raw_ref<base::Clock> clock_{*base::DefaultClock::GetInstance()};

  base::WeakPtrFactory<BtmNavigationFlowDetector> weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_DIPS_DIPS_NAVIGATION_FLOW_DETECTOR_H_

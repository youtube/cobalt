// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TPCD_HEURISTICS_REDIRECT_HEURISTIC_TAB_HELPER_H_
#define CONTENT_BROWSER_TPCD_HEURISTICS_REDIRECT_HEURISTIC_TAB_HELPER_H_

#include <optional>
#include <set>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "content/browser/btm/btm_bounce_detector.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class GURL;

namespace content {

class BtmServiceImpl;
class Page;
class RenderFrameHost;
class WebContents;

class RedirectHeuristicTabHelper
    : public WebContentsObserver,
      public WebContentsUserData<RedirectHeuristicTabHelper>,
      public RedirectChainDetector::Observer {
 public:
  ~RedirectHeuristicTabHelper() override;

  CONTENT_EXPORT static std::set<std::string> AllSitesFollowingFirstParty(
      WebContents* web_contents,
      const GURL& first_party_url);

 private:
  explicit RedirectHeuristicTabHelper(WebContents* web_contents);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class WebContentsUserData<RedirectHeuristicTabHelper>;

  // Record a RedirectHeuristic event for a cookie access, if eligible. This
  // applies when the tracking site has appeared previously in the current
  // redirect context.
  void MaybeRecordRedirectHeuristic(const ukm::SourceId& first_party_source_id,
                                    const CookieAccessDetails& details);
  void RecordRedirectHeuristic(
      const ukm::SourceId& first_party_source_id,
      const ukm::SourceId& third_party_source_id,
      const CookieAccessDetails& details,
      const size_t sites_passed_count,
      bool is_current_interaction,
      BtmInteractionType interaction_type,
      std::optional<base::Time> last_user_interaction_time);

  // Create all eligible RedirectHeuristic grants for the current redirect
  // chain. This may create a storage access grant for any site in the redirect
  // chain on the last committed site, if it meets the criteria.
  void CreateAllRedirectHeuristicGrants(const GURL& first_party_url);
  void CreateRedirectHeuristicGrant(const GURL& url,
                                    const GURL& first_party_url,
                                    base::TimeDelta grant_duration,
                                    bool has_interaction);

  // Start WebContentsObserver overrides:
  void OnCookiesAccessed(RenderFrameHost* render_frame_host,
                         const CookieAccessDetails& details) override;
  void PrimaryPageChanged(Page& page) override;
  void WebContentsDestroyed() override;

  // Start RedirectChainDetector::Observer overrides:
  void OnNavigationCommitted(NavigationHandle* navigation_handle) override;

  raw_ptr<RedirectChainDetector> detector_;
  raw_ptr<BtmServiceImpl> dips_service_;
  raw_ref<base::Clock> clock_{*base::DefaultClock::GetInstance()};
  std::optional<base::Time> last_commit_timestamp_;

  base::ScopedObservation<RedirectChainDetector,
                          RedirectChainDetector::Observer>
      obs_{this};
  base::WeakPtrFactory<RedirectHeuristicTabHelper> weak_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace content

#endif  // CONTENT_BROWSER_TPCD_HEURISTICS_REDIRECT_HEURISTIC_TAB_HELPER_H_

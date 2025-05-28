// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_UNSAFE_RESOURCE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_UNSAFE_RESOURCE_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/security_interstitials/core/unsafe_resource_locator.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "url/gurl.h"

namespace web {
class WebState;
}  // namespace web

namespace security_interstitials {

// Structure that passes parameters between the IO and UI thread when
// interacting with the safe browsing blocking page.
struct UnsafeResource {
  // Helper structure returned through UrlCheckCallback.
  struct UrlCheckResult {
    UrlCheckResult(bool proceed,
                   bool showed_interstitial,
                   bool has_post_commit_interstitial_skipped);
    // Indicates whether or not it is OK to proceed with loading an URL.
    bool proceed;
    // Should only be set to true if the interstitial was shown as a direct
    // result of the navigation to the URL. (e.g. it should be set to true if
    // the interstitial will be shown from a navigation throttle triggered by
    // this navigation, but to false if it will be shown using
    // LoadPostCommitErrorPage). Only used to control the error code returned in
    // BrowserUrlLoaderThrottle.
    bool showed_interstitial;
    // Should only be set to true if LoadPostCommitErrorPage should be called
    // but skipped because the main page load is pending. Only consumed by
    // AsyncCheckTracker.
    bool has_post_commit_interstitial_skipped;
  };

  using UrlCheckCallback = base::RepeatingCallback<void(UrlCheckResult)>;

  using RenderProcessId = UnsafeResourceLocator::RenderProcessId;
  using RenderFrameToken = UnsafeResourceLocator::RenderFrameToken;
  using FrameTreeNodeId = UnsafeResourceLocator::FrameTreeNodeId;
  static constexpr RenderProcessId kNoRenderProcessId =
      UnsafeResourceLocator::kNoRenderProcessId;
  static constexpr FrameTreeNodeId kNoFrameTreeNodeId =
      UnsafeResourceLocator::kNoFrameTreeNodeId;

  UnsafeResource();
  UnsafeResource(const UnsafeResource& other);
  ~UnsafeResource();

  // Returns true if this UnsafeResource is a main frame load while the
  // navigation is still pending. Note that a main frame hit may not be
  // blocking, eg. client side detection happens after the load is committed.
  // Note: If async check is enabled, please call
  // AsyncCheckTracker::IsMainPageLoadPending instead.
  static bool IsMainPageLoadPendingWithSyncCheck(
      safe_browsing::SBThreatType threat_type);

  // Checks if |callback| is not null and posts it to |callback_sequence|.
  void DispatchCallback(const base::Location& from_here,
                        bool proceed,
                        bool showed_interstitial,
                        bool has_post_commit_interstitial_skipped) const;

  GURL url;
  GURL original_url;
  GURL navigation_url;
  GURL referrer_url;
  std::vector<GURL> redirect_urls;
  safe_browsing::SBThreatType threat_type;
  safe_browsing::ThreatMetadata threat_metadata;
  safe_browsing::RTLookupResponse rt_lookup_response;
  // A callback to deliver the |UrlCheckResult| back to the creator of the
  // object. Setting this field is optional to the creator, depending on whether
  // it is interested in knowing the result.
  // This is called back on |callback_sequence|.
  UrlCheckCallback callback;
  scoped_refptr<base::SequencedTaskRunner> callback_sequence;
  // TODO(crbug.com/40686246): `weak_web_state` is only used on iOS, and
  // `rfh_locator` is used on all other platforms. This struct should be
  // refactored to use only the common functionality that can be shared across
  // platforms.
  UnsafeResourceLocator rfh_locator;
  std::optional<int64_t> navigation_id;

  base::WeakPtr<web::WebState> weak_web_state;

  safe_browsing::ThreatSource threat_source =
      safe_browsing::ThreatSource::UNKNOWN;
  // |token| field is only set if |threat_type| is
  // SB_THREAT_TYPE_*_PASSWORD_REUSE.
  std::string token;

  // If true, this UnsafeResource is created because of the Delayed Warnings
  // experiment.
  bool is_delayed_warning;

  // If true, this UnsafeResource is created by a check that doesn't delay
  // navigation to complete. If false, it can either be the UnsafeResource is
  // not for navigation or it delays navigation. Default to false.
  bool is_async_check;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_UNSAFE_RESOURCE_H_

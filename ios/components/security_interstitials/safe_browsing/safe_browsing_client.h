// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_CLIENT_H_
#define IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_CLIENT_H_

#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"

namespace safe_browsing {
class RealTimeUrlLookupService;
}  // namespace safe_browsing

namespace security_interstitials {
struct UnsafeResource;
}  // namespace security_interstitials

namespace web {
class WebState;
}  // namespace web

class GURL;
class SafeBrowsingService;

// Abstract interface to be implemented by clients of ios safe browsing.
class SafeBrowsingClient : public KeyedService {
 public:
  // Returns this as a weak pointer.
  virtual base::WeakPtr<SafeBrowsingClient> AsWeakPtr() = 0;
  // Gets the safe browsing service for this client. Must not be nullptr.
  virtual SafeBrowsingService* GetSafeBrowsingService() = 0;
  // Gets the real time url look up service. Clients may return nullptr.
  virtual safe_browsing::RealTimeUrlLookupService*
  GetRealTimeUrlLookupService() = 0;
  // Returns whether or not `resource` should be blocked from loading.
  virtual bool ShouldBlockUnsafeResource(
      const security_interstitials::UnsafeResource& resource) const = 0;
  // Called when safe browsing decided to cancel loading in the main frame.
  // `web_state` The associated web state.
  // `url` The url which was cancelled.
  virtual void OnMainFrameUrlQueryCancellationDecided(web::WebState* web_state,
                                                      const GURL& url) = 0;
  // Called when safe browsing decided to cancel loading in a sub frame.
  // `web_state` The associated web state.
  // `url` The url which was cancelled.
  // Returns whether or not a blocking page should be displayed.
  virtual bool OnSubFrameUrlQueryCancellationDecided(web::WebState* web_state,
                                                     const GURL& url) = 0;
};

#endif  // IOS_COMPONENTS_SECURITY_INTERSTITIALS_SAFE_BROWSING_SAFE_BROWSING_CLIENT_H_

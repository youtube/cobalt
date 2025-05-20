// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/browser/cobalt_https_only_navigation_throttle.h"
#include "content/public/browser/navigation_handle.h"

namespace content {

CobaltHttpsOnlyNavigationThrottle::CobaltHttpsOnlyNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

CobaltHttpsOnlyNavigationThrottle::~CobaltHttpsOnlyNavigationThrottle() =
    default;

// Called when a network request is about to be made for this navigation.
content::NavigationThrottle::ThrottleCheckResult
CobaltHttpsOnlyNavigationThrottle::WillStartRequest() {
  const GURL& url = navigation_handle()->GetURL();
  if (!(navigation_handle()->GetURL().SchemeIs(url::kHttpsScheme))) {
    LOG(WARNING) << "Navigation throttle canceling navigation due to "
                    "HTTPS-only violation";
    return content::NavigationThrottle::ThrottleCheckResult(
        content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT);
  }
  return content::NavigationThrottle::PROCEED;
}

// Called when a server redirect is received by the navigation.
content::NavigationThrottle::ThrottleCheckResult
CobaltHttpsOnlyNavigationThrottle::WillRedirectRequest() {
  if (!(navigation_handle()->GetURL().SchemeIs(url::kHttpsScheme))) {
    LOG(WARNING) << "Navigation throttle canceling navigation due to "
                    "HTTPS-only violation";
    return content::NavigationThrottle::ThrottleCheckResult(
        content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT);
  }
  return content::NavigationThrottle::PROCEED;
}

const char* CobaltHttpsOnlyNavigationThrottle::GetNameForLogging() {
  return "CobaltHttpsOnlyNavigationThrottle";
}

}  // namespace content

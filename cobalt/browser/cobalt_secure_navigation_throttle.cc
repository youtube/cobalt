// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/browser/cobalt_secure_navigation_throttle.h"
#include "cobalt/browser/switches.h"
#include "content/public/browser/navigation_handle.h"
#include "net/http/http_response_headers.h"

namespace content {

CobaltSecureNavigationThrottle::CobaltSecureNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

CobaltSecureNavigationThrottle::~CobaltSecureNavigationThrottle() = default;

// Called when a network request is about to be made for this navigation.
content::NavigationThrottle::ThrottleCheckResult
CobaltSecureNavigationThrottle::WillStartRequest() {
#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  const GURL& url = navigation_handle()->GetURL();
  if (url.SchemeIs(url::kHttpsScheme)) {
    return content::NavigationThrottle::PROCEED;
  }
  LOG(WARNING) << "Navigation throttle canceling navigation due to "
                  "HTTPS-only violation";
  return content::NavigationThrottle::ThrottleCheckResult(
      content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT);
#endif  // COBALT_IS_OFFICIAL_BUILD

  return content::NavigationThrottle::PROCEED;
}

// Called when a server redirect is received by the navigation.
content::NavigationThrottle::ThrottleCheckResult
CobaltSecureNavigationThrottle::WillRedirectRequest() {
#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  const GURL& url = navigation_handle()->GetURL();
  if (url.SchemeIs(url::kHttpsScheme)) {
    return content::NavigationThrottle::PROCEED;
  }
  LOG(WARNING) << "Navigation throttle canceling navigation due to "
                  "HTTPS-only violation";
  return content::NavigationThrottle::ThrottleCheckResult(
      content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT);
#endif  // COBALT_IS_OFFICIAL_BUILD

  return content::NavigationThrottle::PROCEED;
}

// Called when a response's metadata is available
content::NavigationThrottle::ThrottleCheckResult
CobaltSecureNavigationThrottle::WillProcessResponse() {
  if (ShouldEnforceCSP(*base::CommandLine::ForCurrentProcess())) {
    return EnforceCSPHeaders();
  }
  return content::NavigationThrottle::PROCEED;
}

bool CobaltSecureNavigationThrottle::ShouldEnforceCSP(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(cobalt::switches::kRequireCSP)) {
    return true;
  }
#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  return true;
#endif  // COBALT_IS_OFFICIAL_BUILD
  return false;
}

// Returns a Navigation ThrottleCheckResult based on the
// presence or absence of CSP headers
content::NavigationThrottle::ThrottleCheckResult
CobaltSecureNavigationThrottle::EnforceCSPHeaders() {
  std::string CSP_value;
  const net::HttpResponseHeaders* response_headers =
      navigation_handle()->GetResponseHeaders();
  DCHECK(response_headers);
  if (response_headers == nullptr) {
    LOG(WARNING) << "Navigation throttle canceling navigation due to "
                    "missing response headers";
    return content::NavigationThrottle::ThrottleCheckResult(
        content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT);
  }

  if (response_headers->GetNormalizedHeader("Content-Security-Policy",
                                            &CSP_value)) {
    return content::NavigationThrottle::PROCEED;
  }

  LOG(WARNING) << "Navigation throttle canceling navigation due to "
                  "missing CSP headers";
  return content::NavigationThrottle::ThrottleCheckResult(
      content::NavigationThrottle::CANCEL, net::ERR_BLOCKED_BY_CLIENT);
}

const char* CobaltSecureNavigationThrottle::GetNameForLogging() {
  return "CobaltSecureNavigationThrottle";
}

}  // namespace content

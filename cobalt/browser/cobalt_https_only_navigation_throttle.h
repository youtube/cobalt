// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COBALT_HTTPS_ONLY_NAVIGATION_THROTTLE_H_
#define CONTENT_PUBLIC_BROWSER_COBALT_HTTPS_ONLY_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

// class PrefService;
namespace content {
class CobaltHttpsOnlyNavigationThrottle : public content::NavigationThrottle {
 public:
  CobaltHttpsOnlyNavigationThrottle(content::NavigationHandle* handle);
  ~CobaltHttpsOnlyNavigationThrottle() override;

  CobaltHttpsOnlyNavigationThrottle(const CobaltHttpsOnlyNavigationThrottle&) =
      delete;
  CobaltHttpsOnlyNavigationThrottle& operator=(
      const CobaltHttpsOnlyNavigationThrottle&) = delete;

  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;

  const char* GetNameForLogging() override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COBALT_HTTPS_ONLY_NAVIGATION_THROTTLE_H_

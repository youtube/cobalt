// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COBALT_HTTPS_ONLY_NAVIGATION_THROTTLE_H_
#define CONTENT_PUBLIC_BROWSER_COBALT_HTTPS_ONLY_NAVIGATION_THROTTLE_H_

#include "content/public/browser/navigation_throttle.h"

// class PrefService;
namespace content{
class CobaltHttpsOnlyNavigationThrottle : public content::NavigationThrottle {
  public:
    // static std::unique_ptr<CobaltHttpsOnlyNavigationThrottle>
    // MaybeCreateThrottleFor(content::NavigationHandle* handle/*,
    //     std::unique_ptr<SecurityBlockingPageFactory> blocking_page_factory,
    //     PrefService* prefs*/);

    CobaltHttpsOnlyNavigationThrottle(content::NavigationHandle* handle);
    ~CobaltHttpsOnlyNavigationThrottle() override;

    CobaltHttpsOnlyNavigationThrottle(const CobaltHttpsOnlyNavigationThrottle&) = delete;
    CobaltHttpsOnlyNavigationThrottle& operator=(const CobaltHttpsOnlyNavigationThrottle&) = delete;

    content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
    content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest() override;
    content::NavigationThrottle::ThrottleCheckResult WillFailRequest() override;
    // content::NavigationThrottle::ThrottleCheckResult WillProcessResponse() override;

    const char* GetNameForLogging() override;

    // static void set_timeout_for_testing(int timeout_in_seconds);
};

}  // namespace content

#endif //CONTENT_PUBLIC_BROWSER_COBALT_HTTPS_ONLY_NAVIGATION_THROTTLE_H_
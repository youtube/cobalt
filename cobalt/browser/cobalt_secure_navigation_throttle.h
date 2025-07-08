// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COBALT_SECURE_NAVIGATION_THROTTLE_H_
#define CONTENT_PUBLIC_BROWSER_COBALT_SECURE_NAVIGATION_THROTTLE_H_

#include "base/command_line.h"
#include "content/public/browser/navigation_throttle.h"

// class PrefService;
namespace content {
class CobaltSecureNavigationThrottle : public content::NavigationThrottle {
 public:
  explicit CobaltSecureNavigationThrottle(content::NavigationHandle* handle);
  ~CobaltSecureNavigationThrottle() override;

  CobaltSecureNavigationThrottle(const CobaltSecureNavigationThrottle&) =
      delete;
  CobaltSecureNavigationThrottle& operator=(
      const CobaltSecureNavigationThrottle&) = delete;

  content::NavigationThrottle::ThrottleCheckResult WillStartRequest() override;
  content::NavigationThrottle::ThrottleCheckResult WillRedirectRequest()
      override;
  content::NavigationThrottle::ThrottleCheckResult WillProcessResponse()
      override;

  // Allow the user to disable CSP headers enforcement via a command line
  // parameter.
  bool ShouldEnforceCSP(const base::CommandLine& command_line);
  content::NavigationThrottle::ThrottleCheckResult EnforceCSPHeaders();

  const char* GetNameForLogging() override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COBALT_SECURE_NAVIGATION_THROTTLE_H_

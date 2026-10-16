// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_BROWSER_COBALT_SECURE_NAVIGATION_THROTTLE_H_
#define COBALT_BROWSER_COBALT_SECURE_NAVIGATION_THROTTLE_H_

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

  // Allow the user to enable HTTPS enforcement via a command line parameter.
  virtual bool ShouldEnforceHTTPS(const base::CommandLine& command_line);
  content::NavigationThrottle::ThrottleCheckResult EnforceHTTPS();

  // Allow the user to enable CSP headers enforcement via a command line
  // parameter.
  virtual bool ShouldEnforceCSP(const base::CommandLine& command_line);
  content::NavigationThrottle::ThrottleCheckResult EnforceCSPHeaders();

  const char* GetNameForLogging() override;
};

}  // namespace content

#endif  // COBALT_BROWSER_COBALT_SECURE_NAVIGATION_THROTTLE_H_

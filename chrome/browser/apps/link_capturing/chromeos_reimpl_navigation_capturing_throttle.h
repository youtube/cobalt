// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_REIMPL_NAVIGATION_CAPTURING_THROTTLE_H_
#define CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_REIMPL_NAVIGATION_CAPTURING_THROTTLE_H_

#include "base/auto_reset.h"
#include "content/public/browser/navigation_throttle.h"

class Profile;

namespace base {
class TickClock;
}

namespace apps {

// Navigation throttle that will be used on ChromeOS to implement the parts of
// navigation capturing reimplementation that is not handled by the
// `NavigationCapturingProcess`.
class ChromeOsReimplNavigationCapturingThrottle
    : public content::NavigationThrottle {
 public:
  using ThrottleCheckResult = content::NavigationThrottle::ThrottleCheckResult;

  static std::unique_ptr<content::NavigationThrottle> MaybeCreate(
      content::NavigationHandle* handle);

  // Set clock used for timing to enable manipulation during tests.
  static base::AutoReset<const base::TickClock*> SetClockForTesting(
      const base::TickClock* tick_clock);

  ChromeOsReimplNavigationCapturingThrottle(
      const ChromeOsReimplNavigationCapturingThrottle&) = delete;
  ChromeOsReimplNavigationCapturingThrottle& operator=(
      const ChromeOsReimplNavigationCapturingThrottle&) = delete;
  ~ChromeOsReimplNavigationCapturingThrottle() override;

  // content::NavigationHandle overrides:
  const char* GetNameForLogging() override;
  ThrottleCheckResult WillStartRequest() override;
  ThrottleCheckResult WillRedirectRequest() override;

 private:
  ChromeOsReimplNavigationCapturingThrottle(
      content::NavigationHandle* navigation_handle,
      Profile* profile);

  // Identify whether web contents need to be deleted post navigation capturing.
  bool IsEmptyDanglingWebContentsAfterLinkCapture();

  // Identify if the current navigation was triggered via a link click and not
  // any other sources.
  bool IsCapturableLinkClick();

  raw_ref<Profile> profile_;
  base::WeakPtrFactory<ChromeOsReimplNavigationCapturingThrottle>
      weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_LINK_CAPTURING_CHROMEOS_REIMPL_NAVIGATION_CAPTURING_THROTTLE_H_

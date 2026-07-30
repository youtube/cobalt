// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_EVENT_CONTROLLER_FACTORY_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_EVENT_CONTROLLER_FACTORY_DESKTOP_H_

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/reporting/browser_launch/browser_launch_event_controller.h"

namespace enterprise_reporting {

// Factory to create BrowserLaunchEventController instances on Desktop.
// Manages the wiring of Desktop-specific data collectors and uploaders.
class BrowserLaunchEventControllerFactoryDesktop {
 public:
  // Creates a controller for browser-level reporting.
  static std::unique_ptr<BrowserLaunchEventController> CreateForBrowser();

  // Creates a controller for profile-level reporting.
  static std::unique_ptr<BrowserLaunchEventController> CreateForProfile(
      Profile* profile);

  BrowserLaunchEventControllerFactoryDesktop() = delete;
  ~BrowserLaunchEventControllerFactoryDesktop() = delete;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_BROWSER_LAUNCH_BROWSER_LAUNCH_EVENT_CONTROLLER_FACTORY_DESKTOP_H_

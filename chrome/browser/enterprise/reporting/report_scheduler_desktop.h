// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_DESKTOP_H_

#include "components/enterprise/browser/reporting/report_scheduler.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_observer_factory.h"
#include "chrome/browser/upgrade_detector/build_state_observer.h"

class Profile;

namespace enterprise_reporting {

// Desktop implementation of the ReportScheduler delegate.
class ReportSchedulerDesktop : public ReportScheduler::Delegate,
                               public BuildStateObserver {
 public:
  ReportSchedulerDesktop();
  /* `profile` is used for profile reporting or Chrome OS session.
   * `profile_reporting` should be set to false for Chrome OS only.*/
  explicit ReportSchedulerDesktop(Profile* profile,
                                  bool profile_reporting = false);
  ReportSchedulerDesktop(const ReportSchedulerDesktop&) = delete;
  ReportSchedulerDesktop& operator=(const ReportSchedulerDesktop&) = delete;

  ~ReportSchedulerDesktop() override;

  // ReportScheduler::Delegate implementation.
  PrefService* GetPrefService() override;
  void StartWatchingUpdatesIfNeeded(base::Time last_upload,
                                    base::TimeDelta upload_interval) override;
  void StopWatchingUpdates() override;
  void OnBrowserVersionUploaded() override;

  void StartWatchingExtensionRequestIfNeeded() override;
  void StopWatchingExtensionRequest() override;
  void OnExtensionRequestUploaded() override;
  policy::DMToken GetProfileDMToken() override;
  std::string GetProfileClientId() override;

  // BuildStateObserver implementation.
  void OnUpdate(const BuildState* build_state) override;

  void TriggerExtensionRequest(Profile* profile);

 private:
  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> prefs_;
  std::unique_ptr<ExtensionRequestObserverFactory>
      extension_request_observer_factory_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORT_SCHEDULER_DESKTOP_H_

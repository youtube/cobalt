// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SERVICE_FAKE_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SERVICE_FAKE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/borealis/borealis_install_url_handler.h"
#include "chrome/browser/ash/borealis/borealis_service.h"

namespace content {
class BrowserContext;
}

namespace borealis {

class BorealisServiceFake : public BorealisService {
 public:
  // Causes the service for the given |context| to be a fake in tests. Returns a
  // handle to the fake, which will be owned by the service factory.
  static BorealisServiceFake* UseFakeForTesting(
      content::BrowserContext* context);

  BorealisServiceFake();
  ~BorealisServiceFake() override;

  BorealisAppLauncher& AppLauncher() override;
  BorealisAppUninstaller& AppUninstaller() override;
  BorealisContextManager& ContextManager() override;
  BorealisFeatures& Features() override;
  BorealisInstaller& Installer() override;
  BorealisInstallUrlHandler& InstallUrlHandler() override;
  BorealisLaunchOptions& LaunchOptions() override;
  BorealisShutdownMonitor& ShutdownMonitor() override;
  BorealisWindowManager& WindowManager() override;
  BorealisSurveyHandler& SurveyHandler() override;

  void SetAppLauncherForTesting(BorealisAppLauncher* app_launcher);
  void SetAppUninstallerForTesting(BorealisAppUninstaller* app_uninstaller);
  void SetContextManagerForTesting(BorealisContextManager* context_manager);
  void SetFeaturesForTesting(BorealisFeatures* features);
  void SetInstallerForTesting(BorealisInstaller* installer);
  void SetInstallUrlHandlerForTesting(
      BorealisInstallUrlHandler* install_url_handler);
  void SetLaunchOptionsForTesting(BorealisLaunchOptions* launch_options);
  void SetShutdownMonitorForTesting(BorealisShutdownMonitor* shutdown_monitor);
  void SetWindowManagerForTesting(BorealisWindowManager* window_manager);
  void SetSurveyHandlerForTesting(BorealisSurveyHandler* survey_handler);

 private:
  raw_ptr<BorealisAppLauncher, ExperimentalAsh> app_launcher_ = nullptr;
  raw_ptr<BorealisAppUninstaller, ExperimentalAsh> app_uninstaller_ = nullptr;
  raw_ptr<BorealisContextManager, ExperimentalAsh> context_manager_ = nullptr;
  raw_ptr<BorealisFeatures, DanglingUntriaged | ExperimentalAsh> features_ =
      nullptr;
  raw_ptr<BorealisInstaller, ExperimentalAsh> installer_ = nullptr;
  raw_ptr<BorealisInstallUrlHandler, ExperimentalAsh> install_url_handler_ =
      nullptr;
  raw_ptr<BorealisLaunchOptions, ExperimentalAsh> launch_options_ = nullptr;
  raw_ptr<BorealisShutdownMonitor, DanglingUntriaged | ExperimentalAsh>
      shutdown_monitor_ = nullptr;
  raw_ptr<BorealisWindowManager, DanglingUntriaged | ExperimentalAsh>
      window_manager_ = nullptr;
  raw_ptr<BorealisSurveyHandler, ExperimentalAsh> survey_handler_ = nullptr;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_SERVICE_FAKE_H_

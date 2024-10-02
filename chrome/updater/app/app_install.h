// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_INSTALL_H_
#define CHROME_UPDATER_APP_APP_INSTALL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/lock.h"
#include "chrome/updater/splash_screen.h"

namespace base {
class Version;
}

namespace updater {

class ExternalConstants;
class UpdateService;

// This class defines an interface for installing an application. The interface
// is intended to be implemented for scenerios where UI and RPC calls to
// |UpdateService| are involved, hence the word `controller` in the name of
// the interface.
class AppInstallController
    : public base::RefCountedThreadSafe<AppInstallController> {
 public:
  using Maker = base::RepeatingCallback<scoped_refptr<AppInstallController>(
      scoped_refptr<UpdateService> update_service)>;
  virtual void InstallApp(const std::string& app_id,
                          const std::string& app_name,
                          base::OnceCallback<void(int)> callback) = 0;

  virtual void InstallAppOffline(const std::string& app_id,
                                 const std::string& app_name,
                                 base::OnceCallback<void(int)> callback) = 0;

 protected:
  virtual ~AppInstallController() = default;

 private:
  friend class base::RefCountedThreadSafe<AppInstallController>;
};

// Sets the updater up, shows up a splash screen, then installs an application
// while displaying the UI progress window.
class AppInstall : public App {
 public:
  AppInstall(SplashScreen::Maker splash_screen_maker,
             AppInstallController::Maker app_install_controller_maker);

 private:
  ~AppInstall() override;

  // Overrides for App.
  void Initialize() override;
  void FirstTaskRun() override;

  // Called after the version of the active updater has been retrieved.
  void GetVersionDone(const base::Version& version);

  void InstallCandidateDone(bool valid_version, int result);

  void WakeCandidate();

  void FetchPolicies();

  void RegisterUpdater();

  // Handles the --tag and --app-id command line arguments, and triggers
  // installing of the corresponding application if either argument is present.
  void MaybeInstallApp();

  // Bound to the main sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Inter-process lock taken by AppInstall, AppUninstall, and AppUpdate.
  std::unique_ptr<ScopedLock> setup_lock_;

  // The `app_id_` is parsed from the tag, if the the tag is present, or from
  // the command line argument --app-id.
  std::string app_id_;
  std::string app_name_;

  // Creates instances of |SplashScreen|.
  SplashScreen::Maker splash_screen_maker_;

  // Creates instances of |AppInstallController|.
  AppInstallController::Maker app_install_controller_maker_;

  // The splash screen has a fading effect. That means that the splash screen
  // needs to be alive for a while, until the fading effect is over.
  std::unique_ptr<SplashScreen> splash_screen_;

  scoped_refptr<AppInstallController> app_install_controller_;

  scoped_refptr<ExternalConstants> external_constants_;

  scoped_refptr<UpdateService> update_service_;
};

scoped_refptr<App> MakeAppInstall(bool is_silent_install);

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_INSTALL_H_

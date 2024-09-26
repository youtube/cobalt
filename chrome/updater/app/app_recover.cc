// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_recover.h"

#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/updater/app/app.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/registration_data.h"
#include "chrome/updater/service_proxy_factory.h"
#include "chrome/updater/update_service.h"

namespace updater {

namespace {

class AppRecover : public App {
 public:
  AppRecover(const base::Version& browser_version,
             const std::string& session_id,
             const std::string& browser_app_id)
      : browser_version_(browser_version),
        session_id_(session_id),
        browser_app_id_(browser_app_id) {}

 private:
  ~AppRecover() override = default;
  void Initialize() override;
  void Uninitialize() override;
  void FirstTaskRun() override;

  std::vector<RegistrationRequest> RecordRegisteredApps() const;
  int ReinstallUpdater() const;
  base::FilePath GetUpdaterSetupPath() const;
  void RegisterApps(const std::vector<RegistrationRequest>& registrations,
                    int reinstall_result);

  const base::Version browser_version_;
  const std::string session_id_;  // TODO(crbug.com/1281971): Unused.
  const std::string browser_app_id_;
  scoped_refptr<GlobalPrefs> global_prefs_;
};

void AppRecover::Initialize() {
  global_prefs_ = CreateGlobalPrefs(updater_scope());
}

void AppRecover::Uninitialize() {
  global_prefs_ = nullptr;
}

void AppRecover::FirstTaskRun() {
  if (!global_prefs_) {
    VLOG(0) << "Recovery task could not acquire global prefs.";
    Shutdown(kErrorFailedToLockPrefsMutex);
    return;
  }

  const std::vector<RegistrationRequest> registrations = RecordRegisteredApps();

  // Release global prefs lock so that the updater may run concurrently.
  global_prefs_ = nullptr;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::WithBaseSyncPrimitives()},
      base::BindOnce(&AppRecover::ReinstallUpdater, this),
      base::BindOnce(&AppRecover::RegisterApps, this, registrations));
}

std::vector<RegistrationRequest> AppRecover::RecordRegisteredApps() const {
  CHECK(global_prefs_);
  scoped_refptr<PersistedData> data = base::MakeRefCounted<PersistedData>(
      updater_scope(), global_prefs_->GetPrefService());
  std::vector<RegistrationRequest> apps;
  bool found_browser_registration = false;
  for (const std::string& app : data->GetAppIds()) {
    if (base::EqualsCaseInsensitiveASCII(app, kUpdaterAppId)) {
      continue;
    }
    RegistrationRequest registration;
    registration.app_id = app;
    registration.brand_code = data->GetBrandCode(app);
    registration.brand_path = data->GetBrandPath(app);
    registration.ap = data->GetAP(app);
    registration.existence_checker_path = data->GetExistenceCheckerPath(app);
    if (app == browser_app_id_) {
      found_browser_registration = true;
    }
    if (app == browser_app_id_ && browser_version_.IsValid()) {
      registration.version = browser_version_;
    } else {
      registration.version = data->GetProductVersion(app);
    }
    apps.push_back(registration);
  }
  if (!found_browser_registration) {
    RegistrationRequest registration;
    registration.app_id = browser_app_id_;
    registration.version = browser_version_;
    // TODO(crbug.com/1281971): registration.existence_checker_path must be set.
    apps.emplace_back(registration);
  }
  return apps;
}

int AppRecover::ReinstallUpdater() const {
  base::FilePath setup_path;
  if (!base::PathService::Get(base::FILE_EXE, &setup_path)) {
    LOG(ERROR) << "PathService failed.";
    return kErrorPathServiceFailed;
  }
  int exit_code = -1;
  base::CommandLine uninstall_command(setup_path);
  uninstall_command.AppendSwitch(kUninstallSwitch);
  uninstall_command.AppendSwitch(kEnableLoggingSwitch);
  uninstall_command.AppendSwitchASCII(kLoggingModuleSwitch,
                                      kLoggingModuleSwitchValue);
  if (IsSystemInstall(updater_scope())) {
    uninstall_command.AppendSwitch(kSystemSwitch);
  }
  if (!base::LaunchProcess(uninstall_command, {}).WaitForExit(&exit_code)) {
    VLOG(0) << "Failed to wait for the uninstaller to exit.";
    return kErrorWaitFailedUninstall;
  }
  VLOG(0) << "Uninstaller returned " << exit_code << ".";
  if (exit_code) {
    return exit_code;
  }
  base::CommandLine install_command(setup_path);
  install_command.AppendSwitch(kInstallSwitch);
  install_command.AppendSwitch(kEnableLoggingSwitch);
  install_command.AppendSwitchASCII(kLoggingModuleSwitch,
                                    kLoggingModuleSwitchValue);
  if (IsSystemInstall(updater_scope())) {
    install_command.AppendSwitch(kSystemSwitch);
  }
  // TODO(crbug.com/1281971): suppress the installer's UI.
  if (!base::LaunchProcess(install_command, {}).WaitForExit(&exit_code)) {
    VLOG(0) << "Failed to wait for the installer to exit.";
    return kErrorWaitFailedInstall;
  }
  VLOG(0) << "Installer returned " << exit_code << ".";
  return exit_code;
}

void AppRecover::RegisterApps(
    const std::vector<RegistrationRequest>& registrations,
    int reinstall_result) {
  if (reinstall_result) {
    Shutdown(reinstall_result);
    return;
  }
  scoped_refptr<UpdateService> service =
      CreateUpdateServiceProxy(updater_scope());
  base::RepeatingClosure barrier = base::BarrierClosure(
      registrations.size(),
      // The service is bound to keep it alive through all callbacks.
      base::BindOnce(
          [](scoped_refptr<UpdateService> /*service*/,
             base::OnceClosure shutdown) { std::move(shutdown).Run(); },
          service, base::BindOnce(&AppRecover::Shutdown, this, kErrorOk)));
  for (const RegistrationRequest& registration : registrations) {
    service->RegisterApp(registration,
                         base::BindOnce(
                             [](const std::string& app,
                                base::RepeatingClosure barrier, int result) {
                               VLOG(0) << "Registration for " << app
                                       << " result: " << result << ".";
                               barrier.Run();
                             },
                             registration.app_id, barrier));
  }
}

}  // namespace

scoped_refptr<App> MakeAppRecover() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return base::MakeRefCounted<AppRecover>(
      base::Version(command_line->GetSwitchValueASCII(kBrowserVersionSwitch)),
      command_line->GetSwitchValueASCII(kSessionIdSwitch),
      command_line->GetSwitchValueASCII(kAppGuidSwitch));
}

}  // namespace updater

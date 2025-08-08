// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_INTEGRATION_TEST_COMMANDS_H_
#define CHROME_UPDATER_TEST_INTEGRATION_TEST_COMMANDS_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/update_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace base {
class FilePath;
class Version;
}

namespace updater {
namespace test {

class ScopedServer;

class IntegrationTestCommands
    : public base::RefCountedThreadSafe<IntegrationTestCommands> {
 public:
  virtual void EnterTestMode(const GURL& update_url,
                             const GURL& crash_upload_url,
                             const GURL& device_management_url,
                             const base::TimeDelta& idle_timeout) const = 0;
  virtual void ExitTestMode() const = 0;
  virtual void SetGroupPolicies(const base::Value::Dict& values) const = 0;
  virtual void SetPlatformPolicies(const base::Value::Dict& values) const = 0;
  virtual void SetMachineManaged(bool is_managed_device) const = 0;
  virtual void Clean() const = 0;
  virtual void ExpectClean() const = 0;
  virtual void ExpectInstalled() const = 0;
  virtual void ExpectCandidateUninstalled() const = 0;
  virtual void Install() const = 0;
  virtual void InstallUpdaterAndApp(
      const std::string& app_id,
      const bool is_silent_install,
      const std::string& tag,
      const std::string& child_window_text_to_find) const = 0;
  virtual void SetActive(const std::string& app_id) const = 0;
  virtual void ExpectActive(const std::string& app_id) const = 0;
  virtual void ExpectNotActive(const std::string& app_id) const = 0;
  virtual void ExpectSelfUpdateSequence(ScopedServer* test_server) const = 0;
  virtual void ExpectUninstallPing(ScopedServer* test_server) const = 0;
  virtual void ExpectUpdateCheckRequest(ScopedServer* test_server) const = 0;
  virtual void ExpectUpdateCheckSequence(
      ScopedServer* test_server,
      const std::string& app_id,
      UpdateService::Priority priority,
      const base::Version& from_version,
      const base::Version& to_version) const = 0;
  virtual void ExpectUpdateSequence(ScopedServer* test_server,
                                    const std::string& app_id,
                                    const std::string& install_data_index,
                                    UpdateService::Priority priority,
                                    const base::Version& from_version,
                                    const base::Version& to_version) const = 0;
  virtual void ExpectUpdateSequenceBadHash(
      ScopedServer* test_server,
      const std::string& app_id,
      const std::string& install_data_index,
      UpdateService::Priority priority,
      const base::Version& from_version,
      const base::Version& to_version) const = 0;
  virtual void ExpectInstallSequence(ScopedServer* test_server,
                                     const std::string& app_id,
                                     const std::string& install_data_index,
                                     UpdateService::Priority priority,
                                     const base::Version& from_version,
                                     const base::Version& to_version) const = 0;
  virtual void ExpectVersionActive(const std::string& version) const = 0;
  virtual void ExpectVersionNotActive(const std::string& version) const = 0;
  virtual void Uninstall() const = 0;
  virtual void InstallApp(const std::string& app_id,
                          const base::Version& version) const = 0;
  virtual void ExpectNoCrashes() const = 0;
  virtual void CopyLog() const = 0;
  virtual void SetupFakeUpdaterHigherVersion() const = 0;
  virtual void SetupFakeUpdaterLowerVersion() const = 0;
  virtual void SetupRealUpdaterLowerVersion() const = 0;
  virtual void SetExistenceCheckerPath(const std::string& app_id,
                                       const base::FilePath& path) const = 0;
  virtual void SetServerStarts(int value) const = 0;
  virtual void FillLog() const = 0;
  virtual void ExpectLogRotated() const = 0;
  virtual void ExpectRegistered(const std::string& app_id) const = 0;
  virtual void ExpectNotRegistered(const std::string& app_id) const = 0;
  virtual void ExpectAppTag(const std::string& app_id,
                            const std::string& tag) const = 0;
  virtual void ExpectAppVersion(const std::string& app_id,
                                const base::Version& version) const = 0;
  virtual void RunWake(int exit_code) const = 0;
  virtual void RunWakeAll() const = 0;
  virtual void RunWakeActive(int exit_code) const = 0;
  virtual void RunCrashMe() const = 0;
  virtual void RunServer(int exit_code, bool internal) const = 0;

  virtual void CheckForUpdate(const std::string& app_id) const = 0;
  virtual void Update(const std::string& app_id,
                      const std::string& install_data_index) const = 0;
  virtual void UpdateAll() const = 0;
  virtual void GetAppStates(
      const base::Value::Dict& expected_app_states) const = 0;
  virtual void DeleteUpdaterDirectory() const = 0;
  virtual void DeleteActiveUpdaterExecutable() const = 0;
  virtual void DeleteFile(const base::FilePath& path) const = 0;
  virtual void PrintLog() const = 0;
  virtual base::FilePath GetDifferentUserPath() const = 0;
  [[nodiscard]] virtual bool WaitForUpdaterExit() const = 0;
#if BUILDFLAG(IS_WIN)
  virtual void ExpectInterfacesRegistered() const = 0;
  virtual void ExpectMarshalInterfaceSucceeds() const = 0;
  virtual void ExpectLegacyUpdate3WebSucceeds(
      const std::string& app_id,
      AppBundleWebCreateMode app_bundle_web_create_mode,
      int expected_final_state,
      int expected_error_code) const = 0;
  virtual void ExpectLegacyProcessLauncherSucceeds() const = 0;
  virtual void ExpectLegacyAppCommandWebSucceeds(
      const std::string& app_id,
      const std::string& command_id,
      const base::Value::List& parameters,
      int expected_exit_code) const = 0;
  virtual void ExpectLegacyPolicyStatusSucceeds() const = 0;
  virtual void RunUninstallCmdLine() const = 0;
  virtual void RunHandoff(const std::string& app_id) const = 0;
#endif  // BUILDFLAG(IS_WIN)
  virtual void InstallAppViaService(
      const std::string& app_id,
      const base::Value::Dict& expected_final_values) const = 0;
  virtual void StressUpdateService() const = 0;
  virtual void CallServiceUpdate(const std::string& app_id,
                                 const std::string& install_data_index,
                                 UpdateService::PolicySameVersionUpdate
                                     policy_same_version_update) const = 0;

  virtual void SetupFakeLegacyUpdater() const = 0;
#if BUILDFLAG(IS_WIN)
  virtual void RunFakeLegacyUpdater() const = 0;
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(IS_MAC)
  virtual void PrivilegedHelperInstall() const = 0;
#endif  // BUILDFLAG(IS_WIN)
  virtual void ExpectLegacyUpdaterMigrated() const = 0;
  virtual void RunRecoveryComponent(const std::string& app_id,
                                    const base::Version& version) const = 0;
  virtual void SetLastChecked(const base::Time& time) const = 0;
  virtual void ExpectLastChecked() const = 0;
  virtual void ExpectLastStarted() const = 0;
  virtual void UninstallApp(const std::string& app_id) const = 0;
  virtual void RunOfflineInstall(bool is_legacy_install,
                                 bool is_silent_install) = 0;
  virtual void RunOfflineInstallOsNotSupported(bool is_legacy_install,
                                               bool is_silent_install) = 0;
  virtual void DMPushEnrollmentToken(const std::string& enrollment_token) = 0;
  virtual void DMDeregisterDevice() = 0;
  virtual void DMCleanup() = 0;

 protected:
  friend class base::RefCountedThreadSafe<IntegrationTestCommands>;

  virtual ~IntegrationTestCommands() = default;
};

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommands();

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommandsUser();

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommandsSystem();

}  // namespace test
}  // namespace updater

#endif  // CHROME_UPDATER_TEST_INTEGRATION_TEST_COMMANDS_H_

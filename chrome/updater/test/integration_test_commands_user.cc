// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/test/integration_test_commands.h"
#include "chrome/updater/test/integration_tests_impl.h"
#include "chrome/updater/test/server.h"
#include "chrome/updater/test_scope.h"
#include "chrome/updater/update_service.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace updater {
namespace test {

class IntegrationTestCommandsUser : public IntegrationTestCommands {
 public:
  IntegrationTestCommandsUser() = default;

  void ExpectNoCrashes() const override {
    updater::test::ExpectNoCrashes(updater_scope_);
  }

  void PrintLog() const override { updater::test::PrintLog(updater_scope_); }

  void CopyLog() const override {
    absl::optional<base::FilePath> path = GetInstallDirectory(updater_scope_);
    EXPECT_TRUE(path);
    if (path)
      updater::test::CopyLog(*path);
  }

  void Clean() const override { updater::test::Clean(updater_scope_); }

  void ExpectClean() const override {
    updater::test::ExpectClean(updater_scope_);
  }

  void Install() const override { updater::test::Install(updater_scope_); }

  void InstallUpdaterAndApp(const std::string& app_id) const override {
    updater::test::InstallUpdaterAndApp(updater_scope_, app_id);
  }

  void ExpectInstalled() const override {
    updater::test::ExpectInstalled(updater_scope_);
  }

  void Uninstall() const override { updater::test::Uninstall(updater_scope_); }

  void ExpectCandidateUninstalled() const override {
    updater::test::ExpectCandidateUninstalled(updater_scope_);
  }

  void EnterTestMode(const GURL& update_url,
                     const GURL& crash_upload_url,
                     const GURL& device_management_url) const override {
    updater::test::EnterTestMode(update_url, crash_upload_url,
                                 device_management_url);
  }

  void ExitTestMode() const override {
    updater::test::ExitTestMode(updater_scope_);
  }

  void ExpectSelfUpdateSequence(ScopedServer* test_server) const override {
    updater::test::ExpectSelfUpdateSequence(updater_scope_, test_server);
  }

  void SetGroupPolicies(const base::Value::Dict& values) const override {
    updater::test::SetGroupPolicies(values);
  }

  void ExpectUninstallPing(ScopedServer* test_server) const override {
    updater::test::ExpectUninstallPing(updater_scope_, test_server);
  }

  void ExpectUpdateCheckSequence(
      ScopedServer* test_server,
      const std::string& app_id,
      UpdateService::Priority priority,
      const base::Version& from_version,
      const base::Version& to_version) const override {
    updater::test::ExpectUpdateCheckSequence(updater_scope_, test_server,
                                             app_id, priority, from_version,
                                             to_version);
  }

  void ExpectUpdateSequence(ScopedServer* test_server,
                            const std::string& app_id,
                            const std::string& install_data_index,
                            UpdateService::Priority priority,
                            const base::Version& from_version,
                            const base::Version& to_version) const override {
    updater::test::ExpectUpdateSequence(updater_scope_, test_server, app_id,
                                        install_data_index, priority,
                                        from_version, to_version);
  }

  void ExpectInstallSequence(ScopedServer* test_server,
                             const std::string& app_id,
                             const std::string& install_data_index,
                             UpdateService::Priority priority,
                             const base::Version& from_version,
                             const base::Version& to_version) const override {
    updater::test::ExpectInstallSequence(updater_scope_, test_server, app_id,
                                         install_data_index, priority,
                                         from_version, to_version);
  }

  void ExpectVersionActive(const std::string& version) const override {
    updater::test::ExpectVersionActive(updater_scope_, version);
  }

  void ExpectVersionNotActive(const std::string& version) const override {
    updater::test::ExpectVersionNotActive(updater_scope_, version);
  }

  void SetupFakeUpdaterHigherVersion() const override {
    updater::test::SetupFakeUpdaterHigherVersion(updater_scope_);
  }

  void SetupFakeUpdaterLowerVersion() const override {
    updater::test::SetupFakeUpdaterLowerVersion(updater_scope_);
  }

  void SetupRealUpdaterLowerVersion() const override {
    updater::test::SetupRealUpdaterLowerVersion(updater_scope_);
  }

  void SetExistenceCheckerPath(const std::string& app_id,
                               const base::FilePath& path) const override {
    updater::test::SetExistenceCheckerPath(updater_scope_, app_id, path);
  }

  void SetServerStarts(int value) const override {
    updater::test::SetServerStarts(updater_scope_, value);
  }

  void FillLog() const override { updater::test::FillLog(updater_scope_); }

  void ExpectLogRotated() const override {
    updater::test::ExpectLogRotated(updater_scope_);
  }

  void ExpectRegistered(const std::string& app_id) const override {
    updater::test::ExpectRegistered(updater_scope_, app_id);
  }

  void ExpectNotRegistered(const std::string& app_id) const override {
    updater::test::ExpectNotRegistered(updater_scope_, app_id);
  }

  void ExpectAppVersion(const std::string& app_id,
                        const base::Version& version) const override {
    updater::test::ExpectAppVersion(updater_scope_, app_id, version);
  }

  void SetActive(const std::string& app_id) const override {
    updater::test::SetActive(updater_scope_, app_id);
  }

  void ExpectActive(const std::string& app_id) const override {
    updater::test::ExpectActive(updater_scope_, app_id);
  }

  void ExpectNotActive(const std::string& app_id) const override {
    updater::test::ExpectNotActive(updater_scope_, app_id);
  }

  void RunWake(int exit_code) const override {
    updater::test::RunWake(updater_scope_, exit_code);
  }

  void RunWakeAll() const override {
    updater::test::RunWakeAll(updater_scope_);
  }

  void RunCrashMe() const override {
    updater::test::RunCrashMe(updater_scope_);
  }

  void RunWakeActive(int exit_code) const override {
    updater::test::RunWakeActive(updater_scope_, exit_code);
  }

  void CheckForUpdate(const std::string& app_id) const override {
    updater::test::CheckForUpdate(updater_scope_, app_id);
  }

  void Update(const std::string& app_id,
              const std::string& install_data_index) const override {
    updater::test::Update(updater_scope_, app_id, install_data_index);
  }

  void UpdateAll() const override { updater::test::UpdateAll(updater_scope_); }

  void DeleteUpdaterDirectory() const override {
    updater::test::DeleteUpdaterDirectory(updater_scope_);
  }

  void InstallApp(const std::string& app_id) const override {
    updater::test::InstallApp(updater_scope_, app_id);
  }

  bool WaitForUpdaterExit() const override {
    return updater::test::WaitForUpdaterExit(updater_scope_);
  }

#if BUILDFLAG(IS_WIN)
  void ExpectInterfacesRegistered() const override {
    updater::test::ExpectInterfacesRegistered(updater_scope_);
  }

  void ExpectMarshalInterfaceSucceeds() const override {
    updater::test::ExpectMarshalInterfaceSucceeds(updater_scope_);
  }

  void ExpectLegacyUpdate3WebSucceeds(
      const std::string& app_id,
      AppBundleWebCreateMode app_bundle_web_create_mode,
      int expected_final_state,
      int expected_error_code) const override {
    updater::test::ExpectLegacyUpdate3WebSucceeds(
        updater_scope_, app_id, app_bundle_web_create_mode,
        expected_final_state, expected_error_code);
  }

  void ExpectLegacyProcessLauncherSucceeds() const override {
    updater::test::ExpectLegacyProcessLauncherSucceeds(updater_scope_);
  }

  void ExpectLegacyAppCommandWebSucceeds(
      const std::string& app_id,
      const std::string& command_id,
      const base::Value::List& parameters,
      int expected_exit_code) const override {
    updater::test::ExpectLegacyAppCommandWebSucceeds(
        updater_scope_, app_id, command_id, parameters, expected_exit_code);
  }

  void ExpectLegacyPolicyStatusSucceeds() const override {
    updater::test::ExpectLegacyPolicyStatusSucceeds(updater_scope_);
  }

  void RunUninstallCmdLine() const override {
    updater::test::RunUninstallCmdLine(updater_scope_);
  }

  void SetUpTestService() const override {}

  void TearDownTestService() const override {}

  void RunHandoff(const std::string& app_id) const override {
    updater::test::RunHandoff(updater_scope_, app_id);
  }
#endif  // BUILDFLAG(IS_WIN)

  base::FilePath GetDifferentUserPath() const override {
#if BUILDFLAG(IS_MAC)
    // /Library is owned by root.
    return base::FilePath(FILE_PATH_LITERAL("/Library"));
#else
    NOTREACHED() << __func__ << ": not implemented.";
    return base::FilePath();
#endif
  }

  void StressUpdateService() const override {
    updater::test::StressUpdateService(updater_scope_);
  }

  void CallServiceUpdate(const std::string& app_id,
                         const std::string& install_data_index,
                         UpdateService::PolicySameVersionUpdate
                             policy_same_version_update) const override {
    updater::test::CallServiceUpdate(
        updater_scope_, app_id, install_data_index,
        policy_same_version_update ==
            UpdateService::PolicySameVersionUpdate::kAllowed);
  }

  void SetupFakeLegacyUpdater() const override {
    updater::test::SetupFakeLegacyUpdater(updater_scope_);
  }

#if BUILDFLAG(IS_WIN)
  void RunFakeLegacyUpdater() const override {
    updater::test::RunFakeLegacyUpdater(updater_scope_);
  }
#endif  // BUILDFLAG(IS_WIN)

  void ExpectLegacyUpdaterMigrated() const override {
    updater::test::ExpectLegacyUpdaterMigrated(updater_scope_);
  }

  void RunRecoveryComponent(const std::string& app_id,
                            const base::Version& version) const override {
    updater::test::RunRecoveryComponent(updater_scope_, app_id, version);
  }

  void ExpectLastChecked() const override {
    updater::test::ExpectLastChecked(updater_scope_);
  }

  void ExpectLastStarted() const override {
    updater::test::ExpectLastStarted(updater_scope_);
  }

  void UninstallApp(const std::string& app_id) const override {
    updater::test::UninstallApp(updater_scope_, app_id);
  }

  void RunOfflineInstall(bool is_legacy_install,
                         bool is_silent_install) override {
    updater::test::RunOfflineInstall(updater_scope_, is_legacy_install,
                                     is_silent_install);
  }

 private:
  ~IntegrationTestCommandsUser() override = default;

  static const UpdaterScope updater_scope_;
};

const UpdaterScope IntegrationTestCommandsUser::updater_scope_ = GetTestScope();

scoped_refptr<IntegrationTestCommands> CreateIntegrationTestCommandsUser() {
  return base::MakeRefCounted<IntegrationTestCommandsUser>();
}

}  // namespace test
}  // namespace updater

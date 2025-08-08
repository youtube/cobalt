// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_INTEGRATION_TESTS_IMPL_H_
#define CHROME_UPDATER_TEST_INTEGRATION_TESTS_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/updater/test/server.h"
#include "chrome/updater/update_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace base {
class CommandLine;
class Value;
class Version;
}  // namespace base

namespace updater {
enum class UpdaterScope;
}  // namespace updater

namespace wireless_android_enterprise_devicemanagement {
class OmahaSettingsClientProto;
}  // namespace wireless_android_enterprise_devicemanagement

namespace updater::test {

enum class AppBundleWebCreateMode {
  kCreateApp = 0,
  kCreateInstalledApp = 1,
};

struct AppUpdateExpectation {
  AppUpdateExpectation(const std::string& args,
                       const std::string& app_id,
                       const base::Version& from_version,
                       const base::Version& to_version,
                       bool is_install,
                       bool should_update,
                       bool allow_rollback,
                       const std::string& target_version_prefix,
                       const std::string& target_channel,
                       const base::FilePath& crx_relative_path,
                       bool always_serve_crx = false,
                       const UpdateService::ErrorCategory error_category =
                           UpdateService::ErrorCategory::kService,
                       const int error_code = static_cast<int>(
                           UpdateService::Result::kUpdateCanceled),
                       const int event_type = /*EVENT_UPDATE_COMPLETE=*/3,
                       const std::string& custom_app_response = {});
  AppUpdateExpectation(const AppUpdateExpectation&);
  ~AppUpdateExpectation();

  const std::string args;
  const std::string app_id;
  const base::Version from_version;
  const base::Version to_version;
  const bool is_install;
  const bool should_update;
  const bool allow_rollback;
  const std::string target_version_prefix;
  const std::string target_channel;
  const base::FilePath crx_relative_path;
  const bool always_serve_crx;
  const UpdateService::ErrorCategory error_category;
  const int error_code;
  const int event_type;
  const std::string custom_app_response;
};

// Returns the path to the updater installer program (in the build output
// directory). This is typically the updater setup, or the updater itself for
// the platforms where a setup program is not provided.
base::FilePath GetSetupExecutablePath();

// Returns the non-duplicate, unique names for processes which may be running
// during unit tests.
std::set<base::FilePath::StringType> GetTestProcessNames();

// Ensures test processes are not running after the function is called.
void CleanProcesses();

// Verifies that test processes are not running.
void ExpectCleanProcesses();

// Prints the updater.log file to stdout.
void PrintLog(UpdaterScope scope);

// Removes traces of the updater from the system. It is best to run this at the
// start of each test in case a previous crash or timeout on the machine running
// the test left the updater in an installed or partially installed state.
void Clean(UpdaterScope scope);

// Expects that the system is in a clean state, i.e. no updater is installed and
// no traces of an updater exist. Should be run at the start and end of each
// test.
void ExpectClean(UpdaterScope scope);

// Places the updater into test mode (redirect server URLs and disable CUP).
void EnterTestMode(const GURL& update_url,
                   const GURL& crash_upload_url,
                   const GURL& device_management_url,
                   const base::TimeDelta& idle_timeout);

// Takes the updater our of the test mode by deleting the external constants
// JSON file.
void ExitTestMode(UpdaterScope scope);

// Sets the external constants for group policies.
void SetGroupPolicies(const base::Value::Dict& values);

// Sets platform policies. Platform policy is group policy on Windows, and
// Managed Preferences on macOS.
void SetPlatformPolicies(const base::Value::Dict& values);

// Sets whether the machine is in managed state.
void SetMachineManaged(bool is_managed_device);

// Expects to find no crashes. If there are any crashes, causes the test to
// fail. Copies any crashes found to the isolate directory.
void ExpectNoCrashes(UpdaterScope scope);

// Copies the logs to a location where they can be retrieved by ResultDB.
void CopyLog(const base::FilePath& src_dir);

// Expects that the updater is installed on the system.
void ExpectInstalled(UpdaterScope scope);

// Installs the updater.
void Install(UpdaterScope scope);

// Installs the updater and an app via the command line.
void InstallUpdaterAndApp(UpdaterScope scope,
                          const std::string& app_id,
                          const bool is_silent_install,
                          const std::string& tag,
                          const std::string& child_window_text_to_find);

// Expects that the updater is installed on the system and the specified
// version is active.
void ExpectVersionActive(UpdaterScope scope, const std::string& version);
void ExpectVersionNotActive(UpdaterScope scope, const std::string& version);

// Uninstalls the updater. If the updater was installed during the test, it
// should be uninstalled before the end of the test to avoid having an actual
// live updater on the machine that ran the test.
void Uninstall(UpdaterScope scope);

// Runs the wake client and wait for it to exit. Assert that it exits with
// `exit_code`. The server should exit a few seconds after.
void RunWake(UpdaterScope scope, int exit_code);

// Runs the wake-all client and wait for it to exit. Assert that it exits with
// kErrorOk. The server should exit a few seconds after.
void RunWakeAll(UpdaterScope scope);

// As RunWake, but runs the wake client for whatever version of the server is
// active, rather than kUpdaterVersion.
void RunWakeActive(UpdaterScope scope, int exit_code);

// Starts an updater process with switch `--crash-me`.
void RunCrashMe(UpdaterScope scope);

// Runs the server and waits for it to exit. Assert that it exits with
// `exit_code`.
void RunServer(UpdaterScope scope, int exit_code, bool internal);

// Invokes the active instance's UpdateService::Update (via RPC) for an app.
void Update(UpdaterScope scope,
            const std::string& app_id,
            const std::string& install_data_index);

// Invokes the active instance's UpdateService::CheckForUpdate (via RPC) for an
// app.
void CheckForUpdate(UpdaterScope scope, const std::string& app_id);

// Invokes the active instance's UpdateService::UpdateAll (via RPC).
void UpdateAll(UpdaterScope scope);

// Invokes the active instance's UpdateService::Install (via RPC) for an
// app.
void InstallAppViaService(UpdaterScope scope,
                          const std::string& app_id,
                          const base::Value::Dict& expected_final_values);

void GetAppStates(UpdaterScope updater_scope,
                  const base::Value::Dict& expected_app_states);

// Deletes the file.
void DeleteFile(UpdaterScope scope, const base::FilePath& path);

// Deletes the updater executable directory. Does not do any kind of cleanup
// related to service registration. The intent of this command is to replicate
// a common mode of breaking the updater, so we can test how it recovers.
void DeleteUpdaterDirectory(UpdaterScope scope);

// DeleteActiveUpdaterExecutable is a more narrowly-targeted tool for simulating
// a broken updater. Finds the executable for the active updater (including
// any copy owned by systemd), according to the active version on the global
// prefs file, and deletes it. Does not clean up service registrations, updater
// configuration, app registration, etc.
void DeleteActiveUpdaterExecutable(UpdaterScope scope);

// Runs the command and waits for it to exit or time out.
void Run(UpdaterScope scope,
         base::CommandLine command_line,
         int* exit_code = nullptr);

// Returns the path of the Updater executable.
absl::optional<base::FilePath> GetInstalledExecutablePath(UpdaterScope scope);

// Sets up a fake updater on the system at a version lower than the test.
void SetupFakeUpdaterLowerVersion(UpdaterScope scope);

// Sets up a real updater on the system at a version lower than the test. The
// exact version of the updater is not defined.
void SetupRealUpdaterLowerVersion(UpdaterScope scope);

// Sets up a fake updater on the system at a version higher than the test.
void SetupFakeUpdaterHigherVersion(UpdaterScope scope);

// Expects that this version of updater is uninstalled from the system.
void ExpectCandidateUninstalled(UpdaterScope scope);

// Sets the active bit for `app_id`.
void SetActive(UpdaterScope scope, const std::string& app_id);

// Expects that the active bit for `app_id` is set.
void ExpectActive(UpdaterScope scope, const std::string& app_id);

// Expects that the active bit for `app_id` is unset.
void ExpectNotActive(UpdaterScope scope, const std::string& app_id);

void SetExistenceCheckerPath(UpdaterScope scope,
                             const std::string& app_id,
                             const base::FilePath& path);

void SetServerStarts(UpdaterScope scope, int value);

// Writes lots of data into the log file.
void FillLog(UpdaterScope scope);

// Confirms that an old log file exists and that the current log file is small.
void ExpectLogRotated(UpdaterScope scope);

void ExpectRegistered(UpdaterScope scope, const std::string& app_id);

void ExpectNotRegistered(UpdaterScope scope, const std::string& app_id);

void ExpectAppTag(UpdaterScope scope,
                  const std::string& app_id,
                  const std::string& tag);

void ExpectAppVersion(UpdaterScope scope,
                      const std::string& app_id,
                      const base::Version& version);

void RegisterApp(UpdaterScope scope,
                 const std::string& app_id,
                 const base::Version& version);

[[nodiscard]] bool WaitForUpdaterExit(UpdaterScope scope);

#if BUILDFLAG(IS_WIN)
void ExpectInterfacesRegistered(UpdaterScope scope);
void ExpectMarshalInterfaceSucceeds(UpdaterScope scope);
void ExpectLegacyUpdate3WebSucceeds(
    UpdaterScope scope,
    const std::string& app_id,
    AppBundleWebCreateMode app_bundle_web_create_mode,
    int expected_final_state,
    int expected_error_code);
void ExpectLegacyProcessLauncherSucceeds(UpdaterScope scope);
void ExpectLegacyAppCommandWebSucceeds(UpdaterScope scope,
                                       const std::string& app_id,
                                       const std::string& command_id,
                                       const base::Value::List& parameters,
                                       int expected_exit_code);
void ExpectLegacyPolicyStatusSucceeds(UpdaterScope scope);

// Calls a function defined in test/service/win/rpc_client.py.
// Entries of the `arguments` dictionary should be the function's parameter
// name/value pairs.
void InvokeTestServiceFunction(const std::string& function_name,
                               const base::Value::Dict& arguments);

void RunUninstallCmdLine(UpdaterScope scope);
void RunHandoff(UpdaterScope scope, const std::string& app_id);
#endif  // BUILDFLAG(IS_WIN)

// Returns the number of files in the directory, not including directories,
// links, or dot dot.
int CountDirectoryFiles(const base::FilePath& dir);

void ExpectSelfUpdateSequence(UpdaterScope scope, ScopedServer* test_server);

void ExpectUninstallPing(UpdaterScope scope, ScopedServer* test_server);

void ExpectUpdateCheckRequest(UpdaterScope scope, ScopedServer* test_server);

void ExpectUpdateCheckSequence(UpdaterScope scope,
                               ScopedServer* test_server,
                               const std::string& app_id,
                               UpdateService::Priority priority,
                               const base::Version& from_version,
                               const base::Version& to_version);

void ExpectUpdateSequence(UpdaterScope scope,
                          ScopedServer* test_server,
                          const std::string& app_id,
                          const std::string& install_data_index,
                          UpdateService::Priority priority,
                          const base::Version& from_version,
                          const base::Version& to_version);

void ExpectUpdateSequenceBadHash(UpdaterScope scope,
                                 ScopedServer* test_server,
                                 const std::string& app_id,
                                 const std::string& install_data_index,
                                 UpdateService::Priority priority,
                                 const base::Version& from_version,
                                 const base::Version& to_version);

void ExpectInstallSequence(UpdaterScope scope,
                           ScopedServer* test_server,
                           const std::string& app_id,
                           const std::string& install_data_index,
                           UpdateService::Priority priority,
                           const base::Version& from_version,
                           const base::Version& to_version);

void ExpectAppsUpdateSequence(UpdaterScope scope,
                              ScopedServer* test_server,
                              const base::Value::Dict& request_attributes,
                              const std::vector<AppUpdateExpectation>& apps);

void StressUpdateService(UpdaterScope scope);

void CallServiceUpdate(UpdaterScope updater_scope,
                       const std::string& app_id,
                       const std::string& install_data_index,
                       bool same_version_update_allowed);

void SetupFakeLegacyUpdater(UpdaterScope scope);

#if BUILDFLAG(IS_WIN)
void RunFakeLegacyUpdater(UpdaterScope scope);

// Dismiss the installation completion dialog, then wait for the process
// exit.
void CloseInstallCompleteDialog(const std::wstring& child_window_text_to_find);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
void PrivilegedHelperInstall(UpdaterScope scope);
#endif  // BUILDFLAG(IS_WIN)

void ExpectLegacyUpdaterMigrated(UpdaterScope scope);

void RunRecoveryComponent(UpdaterScope scope,
                          const std::string& app_id,
                          const base::Version& version);

void SetLastChecked(UpdaterScope scope, const base::Time& time);

void ExpectLastChecked(UpdaterScope scope);

void ExpectLastStarted(UpdaterScope scope);

void InstallApp(UpdaterScope scope,
                const std::string& app_id,
                const base::Version& version);

void UninstallApp(UpdaterScope scope, const std::string& app_id);

void RunOfflineInstall(UpdaterScope scope,
                       bool is_legacy_install,
                       bool is_silent_install);

void RunOfflineInstallOsNotSupported(UpdaterScope scope,
                                     bool is_legacy_install,
                                     bool is_silent_install);

base::CommandLine MakeElevated(base::CommandLine command_line);

// Stores a device management enrollment token and deletes any existing
// stored device management token (for the already-enrolled state).
// Requires root permissions.
void DMPushEnrollmentToken(const std::string& enrollment_token);

void DMDeregisterDevice(UpdaterScope scope);

void DMCleanup(UpdaterScope scope);

void ExpectDeviceManagementRegistrationRequest(
    ScopedServer* test_server,
    const std::string& enrollment_token,
    const std::string& dm_token);
void ExpectDeviceManagementPolicyFetchRequest(
    ScopedServer* test_server,
    const std::string& dm_token,
    const ::wireless_android_enterprise_devicemanagement::
        OmahaSettingsClientProto& omaha_settings);
void ExpectDeviceManagementPolicyValidationRequest(ScopedServer* test_server,
                                                   const std::string& dm_token);

}  // namespace updater::test

#endif  // CHROME_UPDATER_TEST_INTEGRATION_TESTS_IMPL_H_

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/setup_main.h"

// Must be before msi.h.
#include <windows.h>

#include <msi.h>
#include <psapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/persistent_histogram_storage.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/memory.h"
#include "base/process/process.h"
#include "base/process/process_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/values.h"
#include "base/version.h"
#include "base/win/process_startup_helper.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "base/win/scoped_handle.h"
#include "base/win/win_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/win_key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/management_service/rotate_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/setup/archive_patch_helper.h"
#include "chrome/installer/setup/brand_behaviors.h"
#include "chrome/installer/setup/buildflags.h"
#include "chrome/installer/setup/downgrade_cleanup.h"
#include "chrome/installer/setup/install.h"
#include "chrome/installer/setup/install_params.h"
#include "chrome/installer/setup/install_worker.h"
#include "chrome/installer/setup/installer_crash_reporting.h"
#include "chrome/installer/setup/installer_state.h"
#include "chrome/installer/setup/launch_chrome.h"
#include "chrome/installer/setup/modify_params.h"
#include "chrome/installer/setup/setup_constants.h"
#include "chrome/installer/setup/setup_install_details.h"
#include "chrome/installer/setup/setup_singleton.h"
#include "chrome/installer/setup/setup_util.h"
#include "chrome/installer/setup/uninstall.h"
#include "chrome/installer/setup/user_experiment.h"
#include "chrome/installer/util/app_command.h"
#include "chrome/installer/util/conditional_work_item_list.h"
#include "chrome/installer/util/delete_after_reboot_helper.h"
#include "chrome/installer/util/delete_old_versions.h"
#include "chrome/installer/util/delete_tree_work_item.h"
#include "chrome/installer/util/google_update_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/html_dialog.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installation_state.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/logging_installer.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/self_cleaning_temp_dir.h"
#include "chrome/installer/util/shell_util.h"
#include "chrome/installer/util/util_constants.h"
#include "components/crash/core/app/crash_switches.h"
#include "components/crash/core/app/run_as_crashpad_handler_win.h"
#include "content/public/common/content_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/installer/util/google_update_util.h"
#endif

using installer::InitialPreferences;
using installer::InstallationState;
using installer::InstallerState;
using installer::ProductState;

namespace {

const wchar_t kSystemPrincipalSid[] = L"S-1-5-18";
const wchar_t kDisplayVersion[] = L"DisplayVersion";
const wchar_t kMsiProductIdPrefix[] = L"EnterpriseProduct";

// Overwrite an existing DisplayVersion as written by the MSI installer
// with the real version number of Chrome.
LONG OverwriteDisplayVersion(const std::wstring& path,
                             const std::wstring& value,
                             REGSAM wowkey) {
  base::win::RegKey key;
  LONG result = 0;
  std::wstring existing;
  if ((result = key.Open(HKEY_LOCAL_MACHINE, path.c_str(),
                         KEY_QUERY_VALUE | KEY_SET_VALUE | wowkey)) !=
      ERROR_SUCCESS) {
    VLOG(1) << "Skipping DisplayVersion update because registry key " << path
            << " does not exist in "
            << (wowkey == KEY_WOW64_64KEY ? "64" : "32") << "bit hive";
    return result;
  }
  if ((result = key.ReadValue(kDisplayVersion, &existing)) != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to set DisplayVersion: " << kDisplayVersion
               << " not found under " << path;
    return result;
  }
  if ((result = key.WriteValue(kDisplayVersion, value.c_str())) !=
      ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to set DisplayVersion: " << kDisplayVersion
               << " could not be written under " << path;
    return result;
  }
  VLOG(1) << "Set DisplayVersion at " << path << " to " << value << " from "
          << existing;
  return ERROR_SUCCESS;
}

LONG OverwriteDisplayVersions(const std::wstring& product,
                              const std::wstring& value) {
  // The version is held in two places.  First change it in the MSI Installer
  // registry entry.  It is held under a "squashed guid" key.
  std::wstring reg_path = base::StringPrintf(
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\"
      L"%ls\\Products\\%ls\\InstallProperties",
      kSystemPrincipalSid, InstallUtil::GuidToSquid(product).c_str());
  LONG result1 = OverwriteDisplayVersion(reg_path, value, KEY_WOW64_64KEY);

  // The display version also exists under the Unininstall registry key with
  // the original guid.  Check both WOW64_64 and WOW64_32.
  reg_path = base::StringPrintf(
      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{%ls}",
      product.c_str());
  // Consider the operation a success if either of these succeeds.
  LONG result2 = OverwriteDisplayVersion(reg_path, value, KEY_WOW64_64KEY);
  LONG result3 = OverwriteDisplayVersion(reg_path, value, KEY_WOW64_32KEY);

  return result1 != ERROR_SUCCESS
             ? result1
             : (result2 != ERROR_SUCCESS ? result3 : ERROR_SUCCESS);
}

// Launches a subprocess of `setup_exe` (the full path to this executable in the
// target installation directory) that will wait for msiexec to finish its work
// and then overwrite the DisplayVersion values in the Windows registry. `id` is
// the MSI product ID and `version` is the new Chrome version. The child will
// run with verbose logging enabled if `verbose_logging` is true.
void DelayedOverwriteDisplayVersions(const base::FilePath& setup_exe,
                                     const std::string& id,
                                     const base::Version& version,
                                     bool verbose_logging) {
  DCHECK(install_static::IsSystemInstall());

  // Create an event to be given to the child process that it will signal
  // immediately before blocking on msiexec's mutex.
  SECURITY_ATTRIBUTES attributes = {};
  attributes.nLength = sizeof(attributes);
  attributes.bInheritHandle = TRUE;
  base::win::ScopedHandle start_event(::CreateEventW(
      &attributes, /*bManualReset=*/TRUE, /*bInitialState=*/FALSE,
      /*lpName=*/nullptr));
  PLOG_IF(ERROR, !start_event.IsValid()) << "Failed to create child event";

  base::CommandLine command_line(setup_exe);
  command_line.AppendSwitchASCII(installer::switches::kSetDisplayVersionProduct,
                                 id);
  command_line.AppendSwitchASCII(installer::switches::kSetDisplayVersionValue,
                                 version.GetString());
  if (start_event.IsValid()) {
    command_line.AppendSwitchNative(
        installer::switches::kStartupEventHandle,
        base::NumberToWString(base::win::HandleToUint32(start_event.Get())));
  }
  InstallUtil::AppendModeAndChannelSwitches(&command_line);
  command_line.AppendSwitch(installer::switches::kSystemLevel);
  if (verbose_logging)
    command_line.AppendSwitch(installer::switches::kVerboseLogging);

  base::LaunchOptions launch_options;
  if (start_event.IsValid())
    launch_options.handles_to_inherit.push_back(start_event.Get());
  launch_options.force_breakaway_from_job_ = true;
  base::Process writer = base::LaunchProcess(command_line, launch_options);
  if (!writer.IsValid()) {
    PLOG(ERROR) << "Failed to set DisplayVersion: "
                << "could not launch subprocess to make desired changes."
                << " <<" << command_line.GetCommandLineString() << ">>";
    return;
  }

  if (!start_event.IsValid())
    return;

  // Wait up to 30 seconds for either the start event to be signaled or for the
  // child process to terminate (i.e., in case it crashes).
  constexpr DWORD kWaitForStartTimeoutMs = 30 * 1000;
  const HANDLE handles[] = {start_event.Get(), writer.Handle()};
  auto wait_result =
      ::WaitForMultipleObjects(std::size(handles), &handles[0],
                               /*bWaitAll=*/FALSE, kWaitForStartTimeoutMs);
  if (wait_result == WAIT_OBJECT_0) {
    VLOG(1) << "Proceeding after waiting for DisplayVersion overwrite child.";
  } else if (wait_result == WAIT_OBJECT_0 + 1) {
    LOG(ERROR) << "Proceeding after unexpected DisplayVersion overwrite "
                  "child termination.";
  } else if (wait_result == WAIT_TIMEOUT) {
    LOG(ERROR) << "Proceeding after unexpected timeout waiting for "
                  "DisplayVersion overwrite child.";
  } else {
    DCHECK_EQ(wait_result, WAIT_FAILED);
    PLOG(ERROR) << "Proceeding after failing to wait for DisplayVersion "
                   "overwrite child";
  }
}

// Waits up to a minute for msiexec to release its mutex and then overwrites
// DisplayVersion in the Windows registry.
LONG OverwriteDisplayVersionsAfterMsiexec(base::win::ScopedHandle startup_event,
                                          const std::wstring& product,
                                          const std::wstring& value) {
  bool adjusted_priority = false;
  bool acquired_mutex = false;
  base::win::ScopedHandle msi_handle(::OpenMutexW(
      SYNCHRONIZE, /*bInheritHandle=*/FALSE, L"Global\\_MSIExecute"));
  if (msi_handle.IsValid()) {
    VLOG(1) << "Blocking to acquire MSI mutex.";

    // Raise the priority class for the process so that it can do its work as
    // soon as possible after acquiring the mutex.
    adjusted_priority =
        ::SetPriorityClass(::GetCurrentProcess(), REALTIME_PRIORITY_CLASS) != 0;

    // Notify the parent process that this one is ready to go.
    if (startup_event.IsValid()) {
      ::SetEvent(startup_event.Get());
      startup_event.Close();
    }

    auto wait_result = ::WaitForSingleObject(msi_handle.Get(), 60 * 1000);
    if (wait_result == WAIT_FAILED) {
      // The handle is valid and was opened with SYNCHRONIZE, so the wait should
      // never fail. If it does, wait ten seconds and proceed with the overwrite
      // to match the old behavior.
      PLOG(ERROR) << "Overwriting DisplayVersion in 10s after failing to wait "
                     "for the MSI mutex";
      base::PlatformThread::Sleep(base::Seconds(10));
    } else if (wait_result == WAIT_ABANDONED || wait_result == WAIT_OBJECT_0) {
      VLOG(1) << "Acquired MSI mutex; overwriting DisplayVersion.";
      acquired_mutex = true;
    } else {
      DCHECK_EQ(wait_result, static_cast<DWORD>(WAIT_TIMEOUT));
      LOG(ERROR) << "Timed out waiting for MSI mutex.";
    }
  } else {
    // The mutex should still be held by msiexec since the parent setup.exe
    // (which is run in the context of a Windows Installer operation) is
    // blocking on this process.
    PLOG(ERROR) << "Overwriting DisplayVersion immediately after failing to "
                   "open the MSI mutex";

    // Notify the parent process that this one is ready to go.
    if (startup_event.IsValid()) {
      ::SetEvent(startup_event.Get());
      startup_event.Close();
    }
  }

  auto result = OverwriteDisplayVersions(product, value);

  if (adjusted_priority)
    ::SetPriorityClass(::GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

  if (acquired_mutex)
    ::ReleaseMutex(msi_handle.Get());

  return result;
}

// Returns nullptr if no compressed archive is available for processing,
// otherwise returns a patch helper configured to uncompress and patch.
std::unique_ptr<installer::ArchivePatchHelper> CreateChromeArchiveHelper(
    const base::FilePath& setup_exe,
    const base::CommandLine& command_line,
    const installer::InstallerState& installer_state,
    const base::FilePath& working_directory,
    installer::UnPackConsumer consumer) {
  // A compressed archive is ordinarily given on the command line by the mini
  // installer. If one was not given, look for chrome.packed.7z next to the
  // running program.
  base::FilePath compressed_archive(
      command_line.GetSwitchValuePath(installer::switches::kInstallArchive));
  bool compressed_archive_specified = !compressed_archive.empty();
  if (!compressed_archive_specified) {
    compressed_archive =
        setup_exe.DirName().Append(installer::kChromeCompressedArchive);
  }

  // Fail if no compressed archive is found.
  if (!base::PathExists(compressed_archive)) {
    if (compressed_archive_specified) {
      LOG(ERROR) << installer::switches::kInstallArchive << "="
                 << compressed_archive.value() << " not found.";
    }
    return nullptr;
  }

  // chrome.7z is either extracted directly from the compressed archive into the
  // working dir or is the target of patching in the working dir.
  base::FilePath target(working_directory.Append(installer::kChromeArchive));
  DCHECK(!base::PathExists(target));

  // Specify an empty path for the patch source since it isn't yet known that
  // one is needed. It will be supplied in UncompressAndPatchChromeArchive if it
  // is.
  return std::make_unique<installer::ArchivePatchHelper>(
      working_directory, compressed_archive, base::FilePath(), target,
      consumer);
}

// Returns the MSI product ID from the ClientState key that is populated for MSI
// installs.  This property is encoded in a value name whose format is
// "EnterpriseProduct<GUID>" where <GUID> is the MSI product id.  <GUID> is in
// the format XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX.  The id will be returned if
// found otherwise this method will return an empty string.
//
// This format is strange and its provenance is shrouded in mystery but it has
// the data we need, so use it.
std::wstring FindMsiProductId(const InstallerState& installer_state) {
  HKEY reg_root = installer_state.root_key();

  base::win::RegistryValueIterator value_iter(
      reg_root, install_static::GetClientStateKeyPath().c_str(),
      KEY_WOW64_32KEY);
  for (; value_iter.Valid(); ++value_iter) {
    std::wstring value_name(value_iter.Name());
    if (base::StartsWith(value_name, kMsiProductIdPrefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      return value_name.substr(std::size(kMsiProductIdPrefix) - 1);
    }
  }
  return std::wstring();
}

// Workhorse for producing an uncompressed archive (chrome.7z) given a
// chrome.packed.7z containing either a patch file based on the version of
// chrome being updated or the full uncompressed archive. Returns true on
// success, in which case |archive_type| is populated based on what was found.
// Returns false on failure, in which case |install_status| contains the error
// code and the result is written to the registry (via WriteInstallerResult).
bool UncompressAndPatchChromeArchive(
    const installer::InstallationState& original_state,
    const installer::InstallerState& installer_state,
    installer::ArchivePatchHelper* archive_helper,
    installer::ArchiveType* archive_type,
    installer::InstallStatus* install_status,
    const base::Version& previous_version) {
  installer_state.SetStage(installer::UNCOMPRESSING);

  // UMA tells us the following about the time required for uncompression as of
  // M75:
  // --- Foreground (<10%) ---
  //   Full archive: 7.5s (50%ile) / 52s (99%ile)
  //   Archive patch: <2s (50%ile) / 10-20s (99%ile)
  // --- Background (>90%) ---
  //   Full archive: 22s (50%ile) / >3m (99%ile)
  //   Archive patch: ~2s (50%ile) / 1.5m - >3m (99%ile)
  //
  // The top unpack failure result with 28 days aggregation (>=0.01%)
  // Setup.Install.LzmaUnPackResult_CompressedChromeArchive
  // 13.50% DISK_FULL
  // 0.67% ERROR_NO_SYSTEM_RESOURCES
  // 0.12% ERROR_IO_DEVICE
  // 0.05% INVALID_HANDLE
  // 0.01% INVALID_LEVEL
  // 0.01% FILE_NOT_FOUND
  // 0.01% LOCK_VIOLATION
  // 0.01% ACCESS_DENIED
  //
  // Setup.Install.LzmaUnPackResult_ChromeArchivePatch
  // 0.09% DISK_FULL
  // 0.01% FILE_NOT_FOUND
  //
  // More information can also be found with metrics:
  // Setup.Install.LzmaUnPackNTSTATUS_CompressedChromeArchive
  // Setup.Install.LzmaUnPackNTSTATUS_ChromeArchivePatch
  if (!archive_helper->Uncompress(nullptr)) {
    *install_status = installer::UNCOMPRESSION_FAILED;
    installer_state.WriteInstallerResult(
        *install_status, IDS_INSTALL_UNCOMPRESSION_FAILED_BASE, nullptr);
    return false;
  }

  // Short-circuit if uncompression produced the uncompressed archive rather
  // than a patch file.
  if (base::PathExists(archive_helper->target())) {
    *archive_type = installer::FULL_ARCHIVE_TYPE;
    return true;
  }

  // Find the installed version's archive to serve as the source for patching.
  base::FilePath patch_source(installer::FindArchiveToPatch(
      original_state, installer_state, previous_version));
  if (patch_source.empty()) {
    LOG(ERROR) << "Failed to find archive to patch.";
    *install_status = installer::DIFF_PATCH_SOURCE_MISSING;
    installer_state.WriteInstallerResult(
        *install_status, IDS_INSTALL_UNCOMPRESSION_FAILED_BASE, nullptr);
    return false;
  }
  archive_helper->set_patch_source(patch_source);

  // UMA tells us the following about the time required for patching as of M75:
  // --- Foreground ---
  //   12s (50%ile) / 3-6m (99%ile)
  // --- Background ---
  //   1m (50%ile) / >60m (99%ile)
  installer_state.SetStage(installer::PATCHING);
  if (!archive_helper->ApplyPatch()) {
    *install_status = installer::APPLY_DIFF_PATCH_FAILED;
    installer_state.WriteInstallerResult(
        *install_status, IDS_INSTALL_UNCOMPRESSION_FAILED_BASE, nullptr);
    return false;
  }

  *archive_type = installer::INCREMENTAL_ARCHIVE_TYPE;
  return true;
}

// Repetitively attempts to delete all files that belong to old versions of
// Chrome from |install_dir|. Waits 15 seconds before the first attempt and 5
// minutes after each unsuccessful attempt. Returns when no files that belong to
// an old version of Chrome remain or when another process tries to acquire the
// SetupSingleton.
installer::InstallStatus RepeatDeleteOldVersions(
    const base::FilePath& install_dir,
    const installer::SetupSingleton& setup_singleton) {
  // The 99th percentile of the number of attempts it takes to successfully
  // delete old versions is 2.75. The 75th percentile is 1.77. 98% of calls to
  // this function will successfully delete old versions.
  // Source: 30 days of UMA data on June 25, 2019.
  constexpr int kMaxNumAttempts = 3;
  int num_attempts = 0;

  while (num_attempts < kMaxNumAttempts) {
    // Wait 15 seconds before the first attempt because trying to delete old
    // files right away is likely to fail. Indeed, this is called in 2
    // occasions:
    // - When the installer fails to delete old files after a not-in-use update:
    //   retrying immediately is likely to fail again.
    // - When executables are successfully renamed on Chrome startup or
    //   shutdown: old files can't be deleted because Chrome is still in use.
    // Wait 5 minutes after an unsuccessful attempt because retrying immediately
    // is likely to fail again.
    const base::TimeDelta max_wait_time =
        num_attempts == 0 ? base::Seconds(15) : base::Minutes(5);
    if (setup_singleton.WaitForInterrupt(max_wait_time)) {
      VLOG(1) << "Exiting --delete-old-versions process because another "
                 "process tries to acquire the SetupSingleton.";
      return installer::SETUP_SINGLETON_RELEASED;
    }

    // Note that Windows 11 22H2 has a bug whereby process priorities are not
    // altered by PROCESS_MODE_BACKGROUND_BEGIN, but I/O and memory priorities
    // still are. See https://crbug.com/1396155 for details.
    base::ScopedClosureRunner restore_priority;
    if (::SetPriorityClass(::GetCurrentProcess(),
                           PROCESS_MODE_BACKGROUND_BEGIN) != 0) {
      // Be aware that a process restoring itself to normal priority from
      // background priority is inherently somewhat of a priority inversion.
      restore_priority.ReplaceClosure(base::BindOnce([]() {
        ::SetPriorityClass(::GetCurrentProcess(), PROCESS_MODE_BACKGROUND_END);
      }));
    }
    const bool delete_old_versions_success =
        installer::DeleteOldVersions(install_dir);
    ++num_attempts;

    if (delete_old_versions_success) {
      VLOG(1) << "Successfully deleted all old files from "
                 "--delete-old-versions process.";
      return installer::DELETE_OLD_VERSIONS_SUCCESS;
    } else if (num_attempts == 1) {
      VLOG(1) << "Failed to delete all old files from --delete-old-versions "
                 "process. Will retry every five minutes.";
    }
  }

  VLOG(1) << "Exiting --delete-old-versions process after retrying too many "
             "times to delete all old files.";
  DCHECK_EQ(num_attempts, kMaxNumAttempts);
  return installer::DELETE_OLD_VERSIONS_TOO_MANY_ATTEMPTS;
}

// This function is called when --rename-chrome-exe option is specified on
// setup.exe command line. This function assumes an in-use update has happened
// for Chrome so there should be files called new_chrome.exe and
// new_chrome_proxy.exe on the file system and a key called 'opv' in the
// registry. This function will move new_chrome.exe to chrome.exe,
// new_chrome_proxy.exe to chrome_proxy.exe and delete 'opv' key in one atomic
// operation. This function also deletes elevation policies associated with the
// old version if they exist. |setup_exe| is the path to the current executable.
installer::InstallStatus RenameChromeExecutables(
    const base::FilePath& setup_exe,
    const InstallationState& original_state,
    InstallerState* installer_state) {
  const base::FilePath& target_path = installer_state->target_path();
  base::FilePath chrome_exe(target_path.Append(installer::kChromeExe));
  base::FilePath chrome_new_exe(target_path.Append(installer::kChromeNewExe));
  base::FilePath chrome_old_exe(target_path.Append(installer::kChromeOldExe));
  base::FilePath chrome_proxy_exe(
      target_path.Append(installer::kChromeProxyExe));
  base::FilePath chrome_proxy_new_exe(
      target_path.Append(installer::kChromeProxyNewExe));
  base::FilePath chrome_proxy_old_exe(
      target_path.Append(installer::kChromeProxyOldExe));

  // Create a temporary backup directory on the same volume as chrome.exe so
  // that moving in-use files doesn't lead to trouble.
  installer::SelfCleaningTempDir temp_path;
  if (!temp_path.Initialize(target_path.DirName(),
                            installer::kInstallTempDir)) {
    PLOG(ERROR)
        << "Failed to create Temp directory "
        << target_path.DirName().Append(installer::kInstallTempDir).value();
    return installer::RENAME_FAILED;
  }
  std::unique_ptr<WorkItemList> install_list(WorkItem::CreateWorkItemList());
  // Move chrome.exe to old_chrome.exe, then move new_chrome.exe to chrome.exe.
  install_list->AddMoveTreeWorkItem(chrome_exe, chrome_old_exe,
                                    temp_path.path(), WorkItem::ALWAYS_MOVE);
  install_list->AddMoveTreeWorkItem(chrome_new_exe, chrome_exe,
                                    temp_path.path(), WorkItem::ALWAYS_MOVE);
  install_list->AddDeleteTreeWorkItem(chrome_new_exe, temp_path.path());

  // Move chrome_proxy.exe to old_chrome_proxy.exe if it exists (a previous
  // installation may not have included it), then move new_chrome_proxy.exe to
  // chrome_proxy.exe.
  std::unique_ptr<WorkItemList> existing_proxy_rename_list(
      WorkItem::CreateConditionalWorkItemList(
          new ConditionRunIfFileExists(chrome_proxy_exe)));
  existing_proxy_rename_list->set_log_message("ExistingProxyRenameItemList");
  existing_proxy_rename_list->AddMoveTreeWorkItem(
      chrome_proxy_exe, chrome_proxy_old_exe, temp_path.path(),
      WorkItem::ALWAYS_MOVE);
  install_list->AddWorkItem(existing_proxy_rename_list.release());
  install_list->AddMoveTreeWorkItem(chrome_proxy_new_exe, chrome_proxy_exe,
                                    temp_path.path(), WorkItem::ALWAYS_MOVE);
  install_list->AddDeleteTreeWorkItem(chrome_proxy_new_exe, temp_path.path());

  AddFinalizeUpdateWorkItems(base::Version(chrome::kChromeVersion),
                             *installer_state, setup_exe, install_list.get());

  // Add work items to delete Chrome's "opv", "cpv", and "cmd" values.
  // TODO(grt): Clean this up; https://crbug.com/577816.
  const HKEY reg_root = installer_state->root_key();
  const std::wstring clients_key = install_static::GetClientsKeyPath();

  install_list->AddDeleteRegValueWorkItem(reg_root, clients_key,
                                          KEY_WOW64_32KEY,
                                          google_update::kRegOldVersionField);
  install_list->AddDeleteRegValueWorkItem(
      reg_root, clients_key, KEY_WOW64_32KEY,
      google_update::kRegCriticalVersionField);
  installer::AppCommand(installer::kCmdRenameChromeExe, {})
      .AddDeleteAppCommandWorkItems(reg_root, install_list.get());
  installer::AppCommand(installer::kCmdAlternateRenameChromeExe, {})
      .AddDeleteAppCommandWorkItems(reg_root, install_list.get());

  if (!installer_state->system_install()) {
    install_list->AddDeleteRegValueWorkItem(
        reg_root, clients_key, KEY_WOW64_32KEY, installer::kCmdRenameChromeExe);
  }

  // If a channel was specified by policy, update the "channel" registry value
  // with it so that the browser knows which channel to use, otherwise delete
  // whatever value that key holds.
  installer::AddChannelWorkItems(reg_root, clients_key, install_list.get());

  // old_chrome.exe is still in use in most cases, so ignore failures here.
  install_list->AddDeleteTreeWorkItem(chrome_old_exe, temp_path.path())
      ->set_best_effort(true);
  install_list->AddDeleteTreeWorkItem(chrome_proxy_old_exe, temp_path.path())
      ->set_best_effort(true);

  installer::InstallStatus ret = installer::RENAME_SUCCESSFUL;
  if (install_list->Do()) {
    installer::LaunchDeleteOldVersionsProcess(setup_exe, *installer_state);
  } else {
    LOG(ERROR) << "Renaming of executables failed. Rolling back any changes.";
    install_list->Rollback();
    ret = installer::RENAME_FAILED;
  }
  // temp_path's dtor will take care of deleting or scheduling itself for
  // deletion at reboot when this scope closes.
  VLOG(1) << "Deleting temporary directory " << temp_path.path().value();

  return ret;
}

// Checks for compatibility between the current state of the system and the
// desired operation.
// Also blocks simultaneous user-level and system-level installs.  In the case
// of trying to install user-level Chrome when system-level exists, the
// existing system-level Chrome is launched.
// When the pre-install conditions are not satisfied, the result is written to
// the registry (via WriteInstallerResult), |status| is set appropriately, and
// false is returned.
bool CheckPreInstallConditions(const InstallationState& original_state,
                               const InstallerState& installer_state,
                               installer::InstallStatus* status) {
  if (!installer_state.system_install()) {
    // This is a user-level installation. Make sure that we are not installing
    // on top of an existing system-level installation.

    const ProductState* user_level_product_state =
        original_state.GetProductState(false);
    const ProductState* system_level_product_state =
        original_state.GetProductState(true);

    // Allow upgrades to proceed so that out-of-date versions are not left
    // around.
    if (user_level_product_state)
      return true;

    // This is a new user-level install...

    if (system_level_product_state) {
      // ... and the product already exists at system-level.
      LOG(ERROR) << "Already installed version "
                 << system_level_product_state->version().GetString()
                 << " at system-level conflicts with this one at user-level.";
      // Instruct Google Update to launch the existing system-level Chrome.
      // There should be no error dialog.
      base::FilePath install_path(
          installer::GetChromeInstallPath(true /* system_install */));
      if (install_path.empty()) {
        // Give up if we failed to construct the install path.
        *status = installer::OS_ERROR;
        installer_state.WriteInstallerResult(*status, IDS_INSTALL_OS_ERROR_BASE,
                                             nullptr);
      } else {
        *status = installer::EXISTING_VERSION_LAUNCHED;
        base::FilePath chrome_exe = install_path.Append(installer::kChromeExe);
        base::CommandLine cmd(chrome_exe);
        cmd.AppendSwitch(switches::kForceFirstRun);
        installer_state.WriteInstallerResult(
            *status, IDS_INSTALL_EXISTING_VERSION_LAUNCHED_BASE, nullptr);
        VLOG(1) << "Launching existing system-level chrome instead.";
        base::LaunchProcess(cmd, base::LaunchOptions());
      }
      return false;
    }
  }

  return true;
}

// Initializes |temp_path| to "Temp" within the target directory, and
// |unpack_path| to a random directory beginning with "source" within
// |temp_path|. Returns false on error.
bool CreateTemporaryAndUnpackDirectories(
    const InstallerState& installer_state,
    installer::SelfCleaningTempDir* temp_path,
    base::FilePath* unpack_path) {
  DCHECK(temp_path && unpack_path);

  if (!temp_path->Initialize(installer_state.target_path().DirName(),
                             installer::kInstallTempDir)) {
    PLOG(ERROR) << "Could not create temporary path.";
    return false;
  }
  VLOG(1) << "Created path " << temp_path->path().value();

  if (!base::CreateTemporaryDirInDir(
          temp_path->path(), installer::kInstallSourceDir, unpack_path)) {
    PLOG(ERROR) << "Could not create temporary path for unpacked archive.";
    return false;
  }

  return true;
}

installer::InstallStatus UninstallProducts(InstallationState& original_state,
                                           InstallerState& installer_state,
                                           const base::FilePath& setup_exe,
                                           const base::CommandLine& cmd_line) {
  // System-level Chrome will be launched via this command if its program gets
  // set below.
  base::CommandLine system_level_cmd(base::CommandLine::NO_PROGRAM);

  if (cmd_line.HasSwitch(installer::switches::kSelfDestruct) &&
      !installer_state.system_install()) {
    const base::FilePath system_exe_path(
        installer::GetChromeInstallPath(true).Append(installer::kChromeExe));
    system_level_cmd.SetProgram(system_exe_path);
  }

  installer::InstallStatus install_status = installer::UNINSTALL_SUCCESSFUL;
  const bool force = cmd_line.HasSwitch(installer::switches::kForceUninstall);
  const bool remove_all =
      !cmd_line.HasSwitch(installer::switches::kDoNotRemoveSharedItems);

  const base::Version current_version(
      installer_state.GetCurrentVersion(original_state));
  const installer::ModifyParams modify_params = {
      installer_state,
      original_state,
      setup_exe,
      current_version,
  };

  install_status = UninstallProduct(modify_params, remove_all, force, cmd_line);

  installer::CleanUpInstallationDirectoryAfterUninstall(
      installer_state.target_path(), setup_exe, &install_status);

  // The app and vendor dirs may now be empty. Make a last-ditch attempt to
  // delete them.
  installer::DeleteChromeDirectoriesIfEmpty(installer_state.target_path());

  // Trigger Active Setup if it was requested for the chrome product. This needs
  // to be done after the UninstallProduct calls as some of them might
  // otherwise terminate the process launched by TriggerActiveSetupCommand().
  if (cmd_line.HasSwitch(installer::switches::kTriggerActiveSetup))
    InstallUtil::TriggerActiveSetupCommand();

  if (!system_level_cmd.GetProgram().empty())
    base::LaunchProcess(system_level_cmd, base::LaunchOptions());

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Tell Google Update that an uninstall has taken place if this install did
  // not originate from the MSI. Google Update has its own logic relating to
  // MSI-driven uninstalls that conflicts with this. Ignore the return value:
  // success or failure of Google Update has no bearing on the success or
  // failure of Chrome's uninstallation.
  if (!installer_state.is_msi())
    google_update::UninstallGoogleUpdate(installer_state.system_install());
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  return install_status;
}

installer::InstallStatus InstallProducts(InstallationState& original_state,
                                         const base::FilePath& setup_exe,
                                         const base::CommandLine& cmd_line,
                                         const InitialPreferences& prefs,
                                         InstallerState* installer_state,
                                         base::FilePath* installer_directory) {
  DCHECK(installer_state);
  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  installer::ArchiveType archive_type = installer::UNKNOWN_ARCHIVE_TYPE;
  installer_state->SetStage(installer::PRECONDITIONS);
  // Remove any legacy "-stage:*" values from the product's "ap" value.
  installer::UpdateInstallStatus(archive_type, install_status);

  // Drop to background processing mode if the process was started below the
  // normal process priority class. This is done here because InstallProducts-
  // Helper has read-only access to the state and because the action also
  // affects everything else that runs below.
  const bool entered_background_mode = installer::AdjustProcessPriority();
  VLOG_IF(1, entered_background_mode) << "Entered background processing mode.";

  if (CheckPreInstallConditions(original_state, *installer_state,
                                &install_status)) {
    VLOG(1) << "Installing to " << installer_state->target_path().value();
    install_status = InstallProductsHelper(original_state, setup_exe, cmd_line,
                                           prefs, *installer_state,
                                           installer_directory, &archive_type);
  } else {
    // CheckPreInstallConditions must set the status on failure.
    DCHECK_NE(install_status, installer::UNKNOWN_STATUS);
  }

  // Delete the initial preferences file if present. Note that we do not care
  // about rollback here and we schedule for deletion on reboot if the delete
  // fails. As such, we do not use DeleteTreeWorkItem.
  if (cmd_line.HasSwitch(installer::switches::kInstallerData)) {
    base::FilePath prefs_path(
        cmd_line.GetSwitchValuePath(installer::switches::kInstallerData));
    if (!base::DeleteFile(prefs_path)) {
      LOG(ERROR) << "Failed deleting initial preferences file "
                 << prefs_path.value()
                 << ", scheduling for deletion after reboot.";
      ScheduleFileSystemEntityForDeletion(prefs_path);
    }
  }

  UpdateInstallStatus(archive_type, install_status);

  return install_status;
}

installer::InstallStatus ShowEulaDialog(const std::wstring& inner_frame) {
  VLOG(1) << "About to show EULA";
  std::wstring eula_path = installer::GetLocalizedEulaResource();
  if (eula_path.empty()) {
    LOG(ERROR) << "No EULA path available";
    return installer::EULA_REJECTED;
  }
  // Newer versions of the caller pass an inner frame parameter that must
  // be given to the html page being launched.
  installer::EulaHTMLDialog dlg(eula_path, inner_frame);
  installer::EulaHTMLDialog::Outcome outcome = dlg.ShowModal();
  if (installer::EulaHTMLDialog::REJECTED == outcome) {
    LOG(ERROR) << "EULA rejected or EULA failure";
    return installer::EULA_REJECTED;
  }
  if (installer::EulaHTMLDialog::ACCEPTED_OPT_IN == outcome) {
    VLOG(1) << "EULA accepted (opt-in)";
    return installer::EULA_ACCEPTED_OPT_IN;
  }
  VLOG(1) << "EULA accepted (no opt-in)";
  return installer::EULA_ACCEPTED;
}

// Creates the sentinel indicating that the EULA was required and has been
// accepted.
bool CreateEulaSentinel() {
  base::FilePath eula_sentinel;
  if (!InstallUtil::GetEulaSentinelFilePath(&eula_sentinel))
    return false;

  return (base::CreateDirectory(eula_sentinel.DirName()) &&
          base::WriteFile(eula_sentinel, ""));
}

installer::InstallStatus RegisterDevChrome(
    const installer::ModifyParams& modify_params,
    const base::CommandLine& cmd_line) {
  const InstallationState& original_state = *modify_params.installation_state;
  const base::FilePath& setup_exe = *modify_params.setup_path;

  // Only proceed with registering a dev chrome if no real Chrome installation
  // of the same install mode is present on this system.
  const ProductState* existing_chrome = original_state.GetProductState(false);
  if (!existing_chrome)
    existing_chrome = original_state.GetProductState(true);
  if (existing_chrome) {
    static const wchar_t kPleaseUninstallYourChromeMessage[] =
        L"You already have a full-installation (non-dev) of %1ls, please "
        L"uninstall it first using Add/Remove Programs in the control panel.";
    std::wstring name(InstallUtil::GetDisplayName());
    std::wstring message(
        base::StringPrintf(kPleaseUninstallYourChromeMessage, name.c_str()));

    LOG(ERROR) << "Aborting operation: another installation of " << name
               << " was found, as a last resort (if the product is not present "
                  "in Add/Remove Programs), try executing: "
               << existing_chrome->uninstall_command().GetCommandLineString();
    MessageBox(nullptr, message.c_str(), nullptr, MB_ICONERROR);
    return installer::INSTALL_FAILED;
  }

  base::FilePath chrome_exe(
      cmd_line.GetSwitchValuePath(installer::switches::kRegisterDevChrome));
  if (chrome_exe.empty())
    chrome_exe = setup_exe.DirName().Append(installer::kChromeExe);
  if (!chrome_exe.IsAbsolute())
    chrome_exe = base::MakeAbsoluteFilePath(chrome_exe);

  installer::InstallStatus status = installer::FIRST_INSTALL_SUCCESS;
  if (base::PathExists(chrome_exe)) {
    // Create the Start menu shortcut and pin it to the Win7+ taskbar.
    ShellUtil::ShortcutProperties shortcut_properties(ShellUtil::CURRENT_USER);
    ShellUtil::AddDefaultShortcutProperties(chrome_exe, &shortcut_properties);
    shortcut_properties.set_pin_to_taskbar(true);
    ShellUtil::CreateOrUpdateShortcut(
        ShellUtil::SHORTCUT_LOCATION_START_MENU_ROOT, shortcut_properties,
        ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS);

    // Register Chrome at user-level and make it default.
    if (ShellUtil::CanMakeChromeDefaultUnattended()) {
      ShellUtil::MakeChromeDefault(ShellUtil::CURRENT_USER, chrome_exe, true);
    } else {
      ShellUtil::ShowMakeChromeDefaultSystemUI(chrome_exe);
    }
  } else {
    LOG(ERROR) << "Path not found: " << chrome_exe.value();
    status = installer::INSTALL_FAILED;
  }
  return status;
}

installer::InstallStatus CreateShortcutsInChildProc(
    const InstallerState& installer_state,
    const InitialPreferences& prefs,
    installer::InstallShortcutLevel install_level,
    installer::InstallShortcutOperation install_operation) {
  // Create shortcut in a child process so that shell crashes don't make the
  // install/update fail. Pass install operation on the command line since
  // it can't be deduced by the child process;

  // Creates shortcuts for Chrome.
  const base::FilePath chrome_exe(
      installer_state.target_path().Append(installer::kChromeExe));

  // Install per-user shortcuts on user-level installs and all-users shortcuts
  // on system-level installs. Note that Active Setup will take care of
  // installing missing per-user shortcuts on system-level install (i.e.,
  // quick launch, taskbar pin, and possibly deleted all-users shortcuts).
  CreateOrUpdateShortcuts(chrome_exe, prefs, install_level, install_operation);
  // TODO(): Plumb shortcut creation failure through and return a failure exit
  // code.
  return installer::CREATE_SHORTCUTS_SUCCESS;
}

// This method processes any command line options that make setup.exe do
// various tasks other than installation (renaming chrome.exe, showing eula
// among others). This function returns true if any such command line option
// has been found and processed (so setup.exe should exit at that point).
bool HandleNonInstallCmdLineOptions(installer::ModifyParams& modify_params,
                                    const base::CommandLine& cmd_line,
                                    const InitialPreferences& prefs,
                                    int* exit_code) {
  installer::InstallerState* installer_state =
      &(*modify_params.installer_state);
  installer::InstallationState* original_state =
      &(*modify_params.installation_state);
  const base::FilePath& setup_exe = *modify_params.setup_path;

  // TODO(gab): Add a local |status| variable which each block below sets;
  // only determine the |exit_code| from |status| at the end (this will allow
  // this method to validate that
  // (!handled || status != installer::UNKNOWN_STATUS)).
  bool handled = true;
  // TODO(tommi): Split these checks up into functions and use a data driven
  // map of switch->function.
  if (cmd_line.HasSwitch(installer::switches::kUpdateSetupExe)) {
    installer_state->SetStage(installer::UPDATING_SETUP);
    installer::InstallStatus status = installer::SETUP_PATCH_FAILED;
    // If --update-setup-exe command line option is given, we apply the given
    // patch to current exe, and store the resulting binary in the path
    // specified by --new-setup-exe. But we need to first unpack the file
    // given in --update-setup-exe.

    const base::FilePath compressed_archive(
        cmd_line.GetSwitchValuePath(installer::switches::kUpdateSetupExe));
    VLOG(1) << "Opening archive " << compressed_archive.value();
    // The top unpack failure result with 28 days aggregation (>=0.01%)
    // Setup.Install.LzmaUnPackResult_SetupExePatch
    // 0.02% PATH_NOT_FOUND
    //
    // More information can also be found with metric:
    // Setup.Install.LzmaUnPackNTSTATUS_SetupExePatch

    // We use the `new_setup_exe` directory as the working directory for
    // `ArchivePatchHelper::UncompressAndPatch`. For System installs, this
    // directory would be under %ProgramFiles% (a directory that only admins can
    // write to by default) and hence a secure location.
    const base::FilePath new_setup_exe(
        cmd_line.GetSwitchValuePath(installer::switches::kNewSetupExe));
    if (installer::ArchivePatchHelper::UncompressAndPatch(
            new_setup_exe.DirName(), compressed_archive, setup_exe,
            new_setup_exe, installer::UnPackConsumer::SETUP_EXE_PATCH)) {
      status = installer::NEW_VERSION_UPDATED;
    }

    *exit_code = InstallUtil::GetInstallReturnCode(status);
    if (*exit_code) {
      LOG(WARNING) << "setup.exe patching failed.";
      installer_state->WriteInstallerResult(status, IDS_SETUP_PATCH_FAILED_BASE,
                                            nullptr);
    }
  } else if (cmd_line.HasSwitch(installer::switches::kShowEula)) {
    // Check if we need to show the EULA. If it is passed as a command line
    // then the dialog is shown and regardless of the outcome setup exits here.
    std::wstring inner_frame =
        cmd_line.GetSwitchValueNative(installer::switches::kShowEula);
    *exit_code = ShowEulaDialog(inner_frame);

    if (installer::EULA_REJECTED != *exit_code) {
      if (GoogleUpdateSettings::SetEulaConsent(*original_state, true))
        CreateEulaSentinel();
    }
  } else if (cmd_line.HasSwitch(installer::switches::kConfigureUserSettings)) {
    // NOTE: Should the work done here, on kConfigureUserSettings, change:
    // kActiveSetupVersion in install_worker.cc needs to be increased for Active
    // Setup to invoke this again for all users of this install.
    installer::InstallStatus status = installer::INVALID_STATE_FOR_OPTION;
    if (installer_state->system_install()) {
      bool force =
          cmd_line.HasSwitch(installer::switches::kForceConfigureUserSettings);
      installer::HandleActiveSetupForBrowser(*installer_state, setup_exe,
                                             force);
      status = installer::INSTALL_REPAIRED;
    } else {
      LOG(DFATAL)
          << "--configure-user-settings is incompatible with user-level";
    }
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kRegisterDevChrome)) {
    installer::InstallStatus status =
        RegisterDevChrome(modify_params, cmd_line);
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowser)) {
    installer::InstallStatus status = installer::UNKNOWN_STATUS;
    // If --register-chrome-browser option is specified, register all Chrome
    // protocol/file associations, as well as register it as a valid browser for
    // Start Menu->Internet shortcut. This switch will also register Chrome as a
    // valid handler for a set of URL protocols that Chrome may become the
    // default handler for, either by the user marking Chrome as the default
    // browser, through the Windows Default Programs control panel settings, or
    // through website use of registerProtocolHandler. These protocols are found
    // in ShellUtil::kPotentialProtocolAssociations.  The
    // --register-url-protocol will additionally register Chrome as a potential
    // handler for the supplied protocol, and is used if a website registers a
    // handler for a protocol not found in
    // ShellUtil::kPotentialProtocolAssociations.  These options should only be
    // used when setup.exe is launched with admin rights. We do not make any
    // user specific changes with this option.
    DCHECK(IsUserAnAdmin());
    base::FilePath chrome_exe(cmd_line.GetSwitchValuePath(
        installer::switches::kRegisterChromeBrowser));
    std::wstring suffix;
    if (cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowserSuffix)) {
      suffix = cmd_line.GetSwitchValueNative(
          installer::switches::kRegisterChromeBrowserSuffix);
    }
    if (cmd_line.HasSwitch(installer::switches::kRegisterURLProtocol)) {
      const std::wstring protocol_associations_value =
          cmd_line.GetSwitchValueNative(
              installer::switches::kRegisterURLProtocol);
      absl::optional<ShellUtil::ProtocolAssociations> protocol_associations =
          ShellUtil::ProtocolAssociations::FromCommandLineArgument(
              protocol_associations_value);

      // ShellUtil::RegisterChromeForProtocol performs all registration
      // done by ShellUtil::RegisterChromeBrowser, as well as registering
      // with Windows as capable of handling the supplied protocol.
      if (protocol_associations.has_value() &&
          ShellUtil::RegisterChromeForProtocols(
              chrome_exe, suffix, protocol_associations.value(), false)) {
        status = installer::IN_USE_UPDATED;
      }
    } else {
      if (ShellUtil::RegisterChromeBrowser(chrome_exe, suffix,
                                           /*elevate_if_not_admin=*/false))
        status = installer::IN_USE_UPDATED;
    }
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kDeleteOldVersions) ||
             cmd_line.HasSwitch(installer::switches::kRenameChromeExe)) {
    std::unique_ptr<installer::SetupSingleton> setup_singleton(
        installer::SetupSingleton::Acquire(
            cmd_line, InitialPreferences::ForCurrentProcess(), original_state,
            installer_state));
    if (!setup_singleton) {
      *exit_code = installer::SETUP_SINGLETON_ACQUISITION_FAILED;
    } else if (cmd_line.HasSwitch(installer::switches::kDeleteOldVersions)) {
      *exit_code = RepeatDeleteOldVersions(installer_state->target_path(),
                                           *setup_singleton);
    } else {
      DCHECK(cmd_line.HasSwitch(installer::switches::kRenameChromeExe));
      *exit_code =
          RenameChromeExecutables(setup_exe, *original_state, installer_state);
    }
  } else if (cmd_line.HasSwitch(
                 installer::switches::kCleanupForDowngradeVersion)) {
    // The version being downgraded to.
    std::string new_version = cmd_line.GetSwitchValueASCII(
        installer::switches::kCleanupForDowngradeVersion);
    std::wstring operation = cmd_line.GetSwitchValueNative(
        installer::switches::kCleanupForDowngradeOperation);
    if (operation == L"cleanup" || operation == L"revert") {
      *exit_code = installer::ProcessCleanupForDowngrade(
          base::Version(new_version), /*revert=*/operation == L"revert");
    } else {
      LOG(ERROR) << "Ignoring \"" << cmd_line.GetCommandLineString()
                 << "\" because of invalid \"operation\" argument.";
      *exit_code = installer::DOWNGRADE_CLEANUP_UNKNOWN_OPERATION;
    }
  } else if (cmd_line.HasSwitch(
                 installer::switches::kRemoveChromeRegistration)) {
    // This is almost reverse of --register-chrome-browser option above.
    // Here we delete Chrome browser registration. This option should only
    // be used when setup.exe is launched with admin rights. We do not
    // make any user specific changes in this option.
    std::wstring suffix;
    if (cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowserSuffix)) {
      suffix = cmd_line.GetSwitchValueNative(
          installer::switches::kRegisterChromeBrowserSuffix);
    }
    installer::InstallStatus tmp = installer::UNKNOWN_STATUS;
    installer::DeleteChromeRegistrationKeys(*installer_state,
                                            HKEY_LOCAL_MACHINE, suffix, &tmp);
    *exit_code = tmp;
  } else if (cmd_line.HasSwitch(installer::switches::kOnOsUpgrade)) {
    installer::InstallStatus status = installer::INVALID_STATE_FOR_OPTION;
    std::unique_ptr<FileVersionInfo> version_info(
        FileVersionInfo::CreateFileVersionInfo(setup_exe));
    const base::Version installed_version(
        base::UTF16ToUTF8(version_info->product_version()));
    if (installed_version.IsValid()) {
      installer::HandleOsUpgradeForBrowser(*installer_state, installed_version,
                                           setup_exe);
      status = installer::INSTALL_REPAIRED;
    } else {
      LOG(DFATAL) << "Failed to extract product version from "
                  << setup_exe.value();
    }
    *exit_code = InstallUtil::GetInstallReturnCode(status);
  } else if (cmd_line.HasSwitch(installer::switches::kUserExperiment)) {
    installer::RunUserExperiment(cmd_line,
                                 InitialPreferences::ForCurrentProcess(),
                                 original_state, installer_state);
    exit_code = 0;
  } else if (cmd_line.HasSwitch(installer::switches::kPatch)) {
    const std::string patch_type_str(
        cmd_line.GetSwitchValueASCII(installer::switches::kPatch));
    const base::FilePath input_file(
        cmd_line.GetSwitchValuePath(installer::switches::kInputFile));
    const base::FilePath patch_file(
        cmd_line.GetSwitchValuePath(installer::switches::kPatchFile));
    const base::FilePath output_file(
        cmd_line.GetSwitchValuePath(installer::switches::kOutputFile));

    if (patch_type_str == installer::kCourgette) {
      *exit_code =
          installer::CourgettePatchFiles(input_file, patch_file, output_file);
    } else if (patch_type_str == installer::kBsdiff) {
      *exit_code =
          installer::BsdiffPatchFiles(input_file, patch_file, output_file);
#if BUILDFLAG(ZUCCHINI)
    } else if (patch_type_str == installer::kZucchini) {
      *exit_code =
          installer::ZucchiniPatchFiles(input_file, patch_file, output_file);
#endif  // BUILDFLAG(ZUCCHINI)
    } else {
      *exit_code = installer::PATCH_INVALID_ARGUMENTS;
    }
  } else if (cmd_line.HasSwitch(installer::switches::kReenableAutoupdates)) {
    // setup.exe has been asked to attempt to reenable updates for Chrome.
    bool updates_enabled = GoogleUpdateSettings::ReenableAutoupdates();
    *exit_code = updates_enabled ? installer::REENABLE_UPDATES_SUCCEEDED
                                 : installer::REENABLE_UPDATES_FAILED;
  } else if (cmd_line.HasSwitch(
                 installer::switches::kSetDisplayVersionProduct)) {
    const std::wstring registry_product(cmd_line.GetSwitchValueNative(
        installer::switches::kSetDisplayVersionProduct));
    const std::wstring registry_value(cmd_line.GetSwitchValueNative(
        installer::switches::kSetDisplayVersionValue));
    uint32_t startup_event_handle_value = 0;
    base::win::ScopedHandle startup_event;
    if (base::StringToUint(cmd_line.GetSwitchValueNative(
                               installer::switches::kStartupEventHandle),
                           &startup_event_handle_value) &&
        startup_event_handle_value) {
      startup_event.Set(base::win::Uint32ToHandle(startup_event_handle_value));
    }

    *exit_code = OverwriteDisplayVersionsAfterMsiexec(
        std::move(startup_event), registry_product, registry_value);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  } else if (cmd_line.HasSwitch(installer::switches::kStoreDMToken)) {
    // Write the specified token to the registry, overwriting any already
    // existing value.
    std::wstring token_switch_value =
        cmd_line.GetSwitchValueNative(installer::switches::kStoreDMToken);
    auto token = installer::DecodeDMTokenSwitchValue(token_switch_value);
    *exit_code = token && installer::StoreDMToken(*token)
                     ? installer::STORE_DMTOKEN_SUCCESS
                     : installer::STORE_DMTOKEN_FAILED;
  } else if (cmd_line.HasSwitch(installer::switches::kDeleteDMToken)) {
    // Delete any existing DMToken from the registry.
    *exit_code = installer::DeleteDMToken() ? installer::DELETE_DMTOKEN_SUCCESS
                                            : installer::DELETE_DMTOKEN_FAILED;
  } else if (cmd_line.HasSwitch(installer::switches::kRotateDeviceTrustKey)) {
    // RotateDeviceTrustKey() expects a single
    // threaded task runner so creating one here.
    base::SingleThreadTaskExecutor executor;

    const auto result = enterprise_connectors::RotateDeviceTrustKey(
        enterprise_connectors::KeyRotationManager::Create(
            std::make_unique<enterprise_connectors::WinKeyNetworkDelegate>()),
        cmd_line, install_static::GetChromeChannel());

    switch (result) {
      case enterprise_connectors::KeyRotationResult::kSucceeded:
        *exit_code = installer::ROTATE_DTKEY_SUCCESS;
        break;
      case enterprise_connectors::KeyRotationResult::kInsufficientPermissions:
        *exit_code = installer::ROTATE_DTKEY_FAILED_PERMISSIONS;
        break;
      case enterprise_connectors::KeyRotationResult::kFailedKeyConflict:
        *exit_code = installer::ROTATE_DTKEY_FAILED_CONFLICT;
        break;
      case enterprise_connectors::KeyRotationResult::kFailed:
        *exit_code = installer::ROTATE_DTKEY_FAILED;
        break;
    }
#endif
  } else if (cmd_line.HasSwitch(installer::switches::kCreateShortcuts)) {
    std::string install_op_arg =
        cmd_line.GetSwitchValueASCII(installer::switches::kCreateShortcuts);
    std::string shortcut_level_arg =
        cmd_line.GetSwitchValueASCII(installer::switches::kInstallLevel);
    int install_op;
    int install_level_op;
    if (!base::StringToInt(install_op_arg, &install_op) ||
        install_op < installer::INSTALL_SHORTCUT_FIRST ||
        install_op > installer::INSTALL_SHORTCUT_LAST) {
      LOG(ERROR) << "Invalid shortcut operation " << install_op_arg;
      *exit_code = installer::UNSUPPORTED_OPTION;
    } else if (!base::StringToInt(shortcut_level_arg, &install_level_op) ||
               install_level_op < installer::INSTALL_SHORTCUT_LEVEL_FIRST ||
               install_level_op > installer::INSTALL_SHORTCUT_LEVEL_LAST) {
      LOG(ERROR) << "Invalid shortcut level " << shortcut_level_arg;
      *exit_code = installer::UNSUPPORTED_OPTION;
    } else {
      *exit_code = CreateShortcutsInChildProc(
          *installer_state, prefs,
          static_cast<installer::InstallShortcutLevel>(install_level_op),
          static_cast<installer::InstallShortcutOperation>(install_op));
    }
  } else {
    handled = false;
  }

  return handled;
}

}  // namespace

namespace installer {

InstallStatus InstallProductsHelper(InstallationState& original_state,
                                    const base::FilePath& setup_exe,
                                    const base::CommandLine& cmd_line,
                                    const InitialPreferences& prefs,
                                    InstallerState& installer_state,
                                    base::FilePath* installer_directory,
                                    ArchiveType* archive_type) {
  DCHECK(archive_type);
  const bool system_install = installer_state.system_install();
  InstallStatus install_status = UNKNOWN_STATUS;

  // Create a temp folder where we will unpack Chrome archive. If it fails,
  // then we are doomed, so return immediately and no cleanup is required.
  SelfCleaningTempDir temp_path;
  base::FilePath unpack_path;
  if (!CreateTemporaryAndUnpackDirectories(installer_state, &temp_path,
                                           &unpack_path)) {
    installer_state.WriteInstallerResult(
        TEMP_DIR_FAILED, IDS_INSTALL_TEMP_DIR_FAILED_BASE, nullptr);
    return TEMP_DIR_FAILED;
  }

  // Uncompress and optionally patch the archive if a compressed archive is
  // found.
  *archive_type = UNKNOWN_ARCHIVE_TYPE;
  base::Version previous_version;
  if (cmd_line.HasSwitch(installer::switches::kPreviousVersion)) {
    previous_version = base::Version(
        cmd_line.GetSwitchValueASCII(installer::switches::kPreviousVersion));
  }

  std::unique_ptr<ArchivePatchHelper> archive_helper(CreateChromeArchiveHelper(
      setup_exe, cmd_line, installer_state, unpack_path,
      (previous_version.IsValid()
           ? UnPackConsumer::CHROME_ARCHIVE_PATCH
           : UnPackConsumer::COMPRESSED_CHROME_ARCHIVE)));
  base::FilePath uncompressed_archive;
  if (archive_helper) {
    VLOG(1) << "Installing Chrome from compressed archive "
            << archive_helper->compressed_archive().value();
    if (!UncompressAndPatchChromeArchive(original_state, installer_state,
                                         archive_helper.get(), archive_type,
                                         &install_status, previous_version)) {
      DCHECK_NE(install_status, UNKNOWN_STATUS);
      return install_status;
    }
    uncompressed_archive = archive_helper->target();
    DCHECK(!uncompressed_archive.empty());
  }

  // Check for an uncompressed archive alongside the current executable if one
  // was not given or generated.
  if (uncompressed_archive.empty())
    uncompressed_archive = setup_exe.DirName().Append(kChromeArchive);

  if (*archive_type == UNKNOWN_ARCHIVE_TYPE) {
    // An archive was not uncompressed or patched above.
    if (uncompressed_archive.empty() ||
        !base::PathExists(uncompressed_archive)) {
      LOG(ERROR) << "Cannot install Chrome without an uncompressed archive.";
      installer_state.WriteInstallerResult(
          INVALID_ARCHIVE, IDS_INSTALL_INVALID_ARCHIVE_BASE, nullptr);
      return INVALID_ARCHIVE;
    }
    *archive_type = FULL_ARCHIVE_TYPE;
  }

  // Unpack the uncompressed archive.
  // UMA tells us the following about the time required to unpack as of M75:
  // --- Foreground ---
  //   <2.7s (50%ile) / 45s (99%ile)
  // --- Background ---
  //   ~14s (50%ile) / >3m (99%ile)
  //
  // The top unpack failure result with 28 days aggregation (>=0.01%)
  // Setup.Install.LzmaUnPackResult_UncompressedChromeArchive
  // 0.66% DISK_FULL
  // 0.04% ACCESS_DENIED
  // 0.01% INVALID_HANDLE
  // 0.01% ERROR_NO_SYSTEM_RESOURCES
  // 0.01% PATH_NOT_FOUND
  // 0.01% ERROR_IO_DEVICE
  //
  // More information can also be found with metric:
  // Setup.Install.LzmaUnPackNTSTATUS_UncompressedChromeArchive
  installer_state.SetStage(UNPACKING);
  UnPackStatus unpack_status = UnPackArchive(uncompressed_archive, unpack_path,
                                             /*output_file=*/nullptr);
  RecordUnPackMetrics(unpack_status,
                      UnPackConsumer::UNCOMPRESSED_CHROME_ARCHIVE);
  if (unpack_status != UNPACK_NO_ERROR) {
    installer_state.WriteInstallerResult(
        UNPACKING_FAILED, IDS_INSTALL_UNCOMPRESSION_FAILED_BASE, nullptr);
    return UNPACKING_FAILED;
  }

  VLOG(1) << "unpacked to " << unpack_path.value();
  base::FilePath src_path(unpack_path.Append(kInstallSourceChromeDir));
  std::unique_ptr<base::Version> installer_version(
      GetMaxVersionFromArchiveDir(src_path));
  if (!installer_version.get()) {
    LOG(ERROR) << "Did not find any valid version in installer.";
    install_status = INVALID_ARCHIVE;
    installer_state.WriteInstallerResult(
        install_status, IDS_INSTALL_INVALID_ARCHIVE_BASE, nullptr);
  } else {
    VLOG(1) << "version to install: " << installer_version->GetString();
    bool proceed_with_installation = true;

    if (!IsDowngradeAllowed(prefs)) {
      const ProductState* product_state =
          original_state.GetProductState(system_install);
      if (product_state != nullptr &&
          (product_state->version().CompareTo(*installer_version) > 0)) {
        LOG(ERROR) << "Higher version of Chrome is already installed.";
        int message_id = IDS_INSTALL_HIGHER_VERSION_BASE;
        proceed_with_installation = false;
        install_status = HIGHER_VERSION_EXISTS;
        installer_state.WriteInstallerResult(install_status, message_id,
                                             nullptr);
      }
    }

    if (proceed_with_installation) {
      base::FilePath prefs_source_path(
          cmd_line.GetSwitchValueNative(switches::kInstallerData));

      const base::Version current_version(
          installer_state.GetCurrentVersion(original_state));
      InstallParams install_params = {
          installer_state,  original_state,       setup_exe,
          current_version,  uncompressed_archive, src_path,
          temp_path.path(), *installer_version,
      };

      install_status =
          InstallOrUpdateProduct(install_params, prefs_source_path, prefs);

      int install_msg_base = IDS_INSTALL_FAILED_BASE;
      base::FilePath chrome_exe;
      std::wstring quoted_chrome_exe;
      if (install_status == SAME_VERSION_REPAIR_FAILED) {
        install_msg_base = IDS_SAME_VERSION_REPAIR_FAILED_BASE;
      } else if (install_status != INSTALL_FAILED) {
        if (installer_state.target_path().empty()) {
          // If we failed to construct install path, it means the OS call to
          // get %ProgramFiles% or %AppData% failed. Report this as failure.
          install_msg_base = IDS_INSTALL_OS_ERROR_BASE;
          install_status = OS_ERROR;
        } else {
          chrome_exe = installer_state.target_path().Append(kChromeExe);
          quoted_chrome_exe =
              GetPostInstallLaunchCommand(installer_state.target_path())
                  .GetCommandLineString();
          install_msg_base = 0;
        }
      }

      installer_state.SetStage(FINISHING);

      bool do_not_register_for_update_launch = false;
      prefs.GetBool(initial_preferences::kDoNotRegisterForUpdateLaunch,
                    &do_not_register_for_update_launch);

      bool write_chrome_launch_string = (!do_not_register_for_update_launch &&
                                         install_status != IN_USE_UPDATED);

      installer_state.WriteInstallerResult(
          install_status, install_msg_base,
          write_chrome_launch_string ? &quoted_chrome_exe : nullptr);

      if (install_status == FIRST_INSTALL_SUCCESS) {
        VLOG(1) << "First install successful.";
        // We never want to launch Chrome in system level install mode.
        bool do_not_launch_chrome = false;
        prefs.GetBool(initial_preferences::kDoNotLaunchChrome,
                      &do_not_launch_chrome);
        if (!system_install && !do_not_launch_chrome)
          LaunchChromeBrowser(installer_state.target_path());
      } else if ((install_status == NEW_VERSION_UPDATED) ||
                 (install_status == IN_USE_UPDATED)) {
        DCHECK_NE(chrome_exe.value(), std::wstring());
        RemoveChromeLegacyRegistryKeys(chrome_exe);
      }
    }
  }

  // If the installation completed successfully...
  if (InstallUtil::GetInstallReturnCode(install_status) == 0) {
    // Update the DisplayVersion created by an MSI-based install.
    base::FilePath initial_preferences_file(
        installer_state.target_path().AppendASCII(
            installer::kLegacyInitialPrefs));
    std::string install_id;
    if (prefs.GetString(installer::initial_preferences::kMsiProductId,
                        &install_id)) {
      // A currently active MSI install will have specified the initial-
      // preferences file on the command-line that includes the product-id.
      // We must delay the setting of the DisplayVersion until after the
      // grandparent "msiexec" process has exited.
      base::FilePath new_setup =
          installer_state.GetInstallerDirectory(*installer_version)
              .Append(kSetupExe);
      DelayedOverwriteDisplayVersions(new_setup, install_id, *installer_version,
                                      installer_state.verbose_logging());
    } else {
      // Only when called by the MSI installer do we need to delay setting
      // the DisplayVersion.  In other runs, such as those done by the auto-
      // update action, we set the value immediately.
      // Get the app's MSI Product-ID from an entry in ClientState.
      std::wstring app_guid = FindMsiProductId(installer_state);
      if (!app_guid.empty()) {
        OverwriteDisplayVersions(
            app_guid, base::UTF8ToWide(installer_version->GetString()));
      }
    }
    // Return the path to the directory containing the newly installed
    // setup.exe and uncompressed archive if the caller requested it.
    if (installer_directory) {
      *installer_directory =
          installer_state.GetInstallerDirectory(*installer_version);
    }
  }

  // temp_path's dtor will take care of deleting or scheduling itself for
  // deletion at reboot when this scope closes.
  VLOG(1) << "Deleting temporary directory " << temp_path.path().value();

  return install_status;
}

}  // namespace installer

int WINAPI wWinMain(HINSTANCE instance,
                    HINSTANCE prev_instance,
                    wchar_t* command_line,
                    int show_command) {
  // Check to see if the CPU is supported before doing anything else. There's
  // very little than can safely be accomplished if the CPU isn't supported
  // since dependent libraries (e.g., base) may use invalid instructions.
  if (!installer::IsProcessorSupported())
    return installer::CPU_NOT_SUPPORTED;

  // Persist histograms so they can be uploaded later. The storage directory is
  // created during installation when the main WorkItemList is evaluated so
  // disable storage directory creation in PersistentHistogramStorage.
  base::PersistentHistogramStorage persistent_histogram_storage(
      installer::kSetupHistogramAllocatorName,
      base::PersistentHistogramStorage::StorageDirManagement::kUseExisting);

  // The exit manager is in charge of calling the dtors of singletons.
  base::AtExitManager exit_manager;
  base::CommandLine::Init(0, nullptr);

  std::string process_type =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kProcessType);

  if (process_type == crash_reporter::switches::kCrashpadHandler) {
    // Histogram storage is enabled at the very top of this wWinMain. Disable it
    // when this process is decicated to crashpad as there is no directory in
    // which to write them nor a browser to subsequently upload them.
    persistent_histogram_storage.Disable();
    return crash_reporter::RunAsCrashpadHandler(
        *base::CommandLine::ForCurrentProcess(), base::FilePath(),
        switches::kProcessType, switches::kUserDataDir);
  }

  // install_util uses chrome paths.
  chrome::RegisterPathProvider();

  const InitialPreferences& prefs = InitialPreferences::ForCurrentProcess();
  installer::InitInstallerLogging(prefs);

  const base::CommandLine& cmd_line = *base::CommandLine::ForCurrentProcess();
  VLOG(1) << "Command Line: " << cmd_line.GetCommandLineString();

  InitializeInstallDetails(cmd_line, prefs);

  bool system_install = false;
  prefs.GetBool(installer::initial_preferences::kSystemLevel, &system_install);
  VLOG(1) << "system install is " << system_install;

  InstallationState original_state;
  original_state.Initialize();

  InstallerState installer_state;
  installer_state.Initialize(cmd_line, prefs, original_state);

  persistent_histogram_storage.set_storage_base_dir(
      installer_state.target_path());

  installer::ConfigureCrashReporting(installer_state);
  installer::SetInitialCrashKeys(installer_state);
  installer::SetCrashKeysFromCommandLine(cmd_line);

  // Make sure the process exits cleanly on unexpected errors.
  base::EnableTerminationOnHeapCorruption();
  base::EnableTerminationOnOutOfMemory();
  base::win::RegisterInvalidParamHandler();
  base::win::SetupCRT(cmd_line);

#if defined(ARCH_CPU_64_BITS) || defined(NDEBUG)
  // Disable the handle verifier for all but 32-bit debug builds.
  base::win::DisableHandleVerifier();
#endif

  const bool is_uninstall = cmd_line.HasSwitch(installer::switches::kUninstall);

  // Histogram storage is enabled at the very top of this wWinMain. Disable it
  // during uninstall since there's neither a directory in which to write them
  // nor a browser to subsequently upload them.
  if (is_uninstall)
    persistent_histogram_storage.Disable();

  // Check to make sure current system is Win10 or later. If not, log
  // error message and get out.
  if (!InstallUtil::IsOSSupported()) {
    LOG(ERROR) << "Chrome only supports Windows 10 or later.";
    installer_state.WriteInstallerResult(installer::OS_NOT_SUPPORTED,
                                         IDS_INSTALL_OS_NOT_SUPPORTED_BASE,
                                         nullptr);
    return installer::OS_NOT_SUPPORTED;
  }

  // Initialize COM for use later.
  base::win::ScopedCOMInitializer com_initializer;
  if (!com_initializer.Succeeded()) {
    installer_state.WriteInstallerResult(installer::OS_ERROR,
                                         IDS_INSTALL_OS_ERROR_BASE, nullptr);
    return installer::OS_ERROR;
  }

  // Make sure system_level is supported if requested. For historical reasons,
  // system-level installs have never been supported for Chrome canary (SxS).
  // This is a brand-specific policy for this particular mode. In general,
  // system-level installation of secondary install modes is fully supported.
  if (!install_static::InstallDetails::Get().supports_system_level() &&
      (system_install ||
       cmd_line.HasSwitch(installer::switches::kSelfDestruct) ||
       cmd_line.HasSwitch(installer::switches::kRemoveChromeRegistration))) {
    return installer::SXS_OPTION_NOT_SUPPORTED;
  }
  // Some switches only apply for modes that can be made the user's default
  // browser.
  if (!install_static::SupportsSetAsDefaultBrowser() &&
      (cmd_line.HasSwitch(installer::switches::kMakeChromeDefault) ||
       cmd_line.HasSwitch(installer::switches::kRegisterChromeBrowser))) {
    return installer::SXS_OPTION_NOT_SUPPORTED;
  }
  // Some switches only apply for modes that support retention experiments.
  if (!install_static::SupportsRetentionExperiments() &&
      cmd_line.HasSwitch(installer::switches::kUserExperiment)) {
    return installer::SXS_OPTION_NOT_SUPPORTED;
  }

  // Some command line options are no longer supported and must error out.
  if (installer::ContainsUnsupportedSwitch(cmd_line))
    return installer::UNSUPPORTED_OPTION;

  // A variety of installer operations require the path to the current
  // executable. Get it once here for use throughout these operations. Note that
  // the path service is the authoritative source for this path. One might think
  // that CommandLine::GetProgram would suffice, but it won't since
  // CreateProcess may have been called with a command line that is somewhat
  // ambiguous (e.g., an unquoted path with spaces, or a path lacking the file
  // extension), in which case CommandLineToArgv will not yield an argv with the
  // true path to the program at position 0.
  base::FilePath setup_exe;
  base::PathService::Get(base::FILE_EXE, &setup_exe);

  const base::Version current_version(
      installer_state.GetCurrentVersion(original_state));
  installer::ModifyParams modify_params = {
      installer_state,
      original_state,
      setup_exe,
      current_version,
  };

  int exit_code = 0;
  if (HandleNonInstallCmdLineOptions(modify_params, cmd_line, prefs,
                                     &exit_code)) {
    return exit_code;
  }

  if (system_install && !IsUserAnAdmin()) {
    if (!cmd_line.HasSwitch(installer::switches::kRunAsAdmin)) {
      base::CommandLine new_cmd(base::CommandLine::NO_PROGRAM);
      new_cmd.AppendArguments(cmd_line, true);
      // Append --run-as-admin flag to let the new instance of setup.exe know
      // that we already tried to launch ourselves as admin.
      new_cmd.AppendSwitch(installer::switches::kRunAsAdmin);
      // If system_install became true due to an environment variable, append
      // it to the command line here since env vars may not propagate past the
      // elevation.
      if (!new_cmd.HasSwitch(installer::switches::kSystemLevel))
        new_cmd.AppendSwitch(installer::switches::kSystemLevel);

      DWORD exe_exit_code = installer::UNKNOWN_STATUS;
      InstallUtil::ExecuteExeAsAdmin(new_cmd, &exe_exit_code);
      return exe_exit_code;
    } else {
      LOG(ERROR) << "Non admin user can not install system level Chrome.";
      installer_state.WriteInstallerResult(installer::INSUFFICIENT_RIGHTS,
                                           IDS_INSTALL_INSUFFICIENT_RIGHTS_BASE,
                                           nullptr);
      return installer::INSUFFICIENT_RIGHTS;
    }
  }

  std::unique_ptr<installer::SetupSingleton> setup_singleton(
      installer::SetupSingleton::Acquire(cmd_line, prefs, &original_state,
                                         &installer_state));
  if (!setup_singleton) {
    installer_state.WriteInstallerResult(
        installer::SETUP_SINGLETON_ACQUISITION_FAILED,
        IDS_INSTALL_SINGLETON_ACQUISITION_FAILED_BASE, nullptr);
    return installer::SETUP_SINGLETON_ACQUISITION_FAILED;
  }

  base::FilePath installer_directory;
  installer::InstallStatus install_status = installer::UNKNOWN_STATUS;
  // If --uninstall option is given, uninstall the identified product(s)
  if (is_uninstall) {
    install_status =
        UninstallProducts(original_state, installer_state, setup_exe, cmd_line);
  } else {
    // If --uninstall option is not specified, we assume it is install case.
    install_status = InstallProducts(original_state, setup_exe, cmd_line, prefs,
                                     &installer_state, &installer_directory);
    DoLegacyCleanups(installer_state, install_status);

    // It may be time to kick off an experiment if this was a successful update
    // and Chrome was not in use (since the experiment only applies to inactive
    // installs).
    if (install_status == installer::NEW_VERSION_UPDATED &&
        installer::ShouldRunUserExperiment(installer_state)) {
      installer::BeginUserExperiment(
          installer_state, installer_directory.Append(setup_exe.BaseName()),
          !system_install);
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Setup.Install.Result", install_status,
                            installer::MAX_INSTALL_STATUS);

  // Dump peak memory usage.
  PROCESS_MEMORY_COUNTERS pmc;
  if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc))) {
    UMA_HISTOGRAM_MEMORY_KB("Setup.Install.PeakPagefileUsage",
                            base::saturated_cast<base::HistogramBase::Sample>(
                                pmc.PeakPagefileUsage / 1024));
    UMA_HISTOGRAM_MEMORY_KB("Setup.Install.PeakWorkingSetSize",
                            base::saturated_cast<base::HistogramBase::Sample>(
                                pmc.PeakWorkingSetSize / 1024));
  }
  auto process_metrics = base::ProcessMetrics::CreateCurrentProcessMetrics();
  auto disk_usage = process_metrics->GetCumulativeDiskUsageInBytes();
  base::UmaHistogramMemoryMB(
      "Setup.Install.CumulativeDiskUsage2",
      base::saturated_cast<int>(base::ClampAdd(disk_usage, 1024 * 1024 / 2) /
                                (1024 * 1024)));

  int return_code = 0;
  // MSI demands that custom actions always return 0 (ERROR_SUCCESS) or it will
  // rollback the action. If we're uninstalling we want to avoid this, so always
  // report success, squashing any more informative return codes.
  if (!(installer_state.is_msi() && is_uninstall)) {
    // Note that we allow the status installer::UNINSTALL_REQUIRES_REBOOT
    // to pass through, since this is only returned on uninstall which is
    // never invoked directly by Google Update.
    return_code = InstallUtil::GetInstallReturnCode(install_status);
  }

  VLOG(1) << "Installation complete, returning: " << return_code;

  return return_code;
}

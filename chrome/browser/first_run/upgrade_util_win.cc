// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/upgrade_util_win.h"

// Must be first.
#include <windows.h>

#include <objbase.h>
#include <psapi.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <ios>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/registry.h"
#include "base/win/windows_version.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/upgrade_util.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/win/browser_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/app_command.h"
#include "chrome/installer/util/util_constants.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ui_base_switches.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "google_update/google_update_idl.h"
#endif

namespace {

bool GetNewerChromeFile(base::FilePath* path) {
  if (!base::PathService::Get(base::DIR_EXE, path))
    return false;
  *path = path->Append(installer::kChromeNewExe);
  return true;
}

bool InvokeGoogleUpdateForRename() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // This has been identified as very slow on some startups. Detailed trace
  // events below try to shine a light on each steps. crbug.com/1252004
  TRACE_EVENT0("startup", "upgrade_util::InvokeGoogleUpdateForRename");

  Microsoft::WRL::ComPtr<IProcessLauncher> ipl;
  {
    TRACE_EVENT0("startup", "InvokeGoogleUpdateForRename CoCreateInstance");
    HRESULT hr = ::CoCreateInstance(__uuidof(ProcessLauncherClass), nullptr,
                                    CLSCTX_ALL, IID_PPV_ARGS(&ipl));
    if (FAILED(hr)) {
      TRACE_EVENT0("startup",
                   "InvokeGoogleUpdateForRename CoCreateInstance failed");
      LOG(ERROR) << "CoCreate ProcessLauncherClass failed; hr = " << std::hex
                 << hr;
      return false;
    }
  }

  ULONG_PTR process_handle = 0;
  {
    TRACE_EVENT0("startup", "InvokeGoogleUpdateForRename LaunchCmdElevated");
    HRESULT hr = ipl->LaunchCmdElevated(
        install_static::GetAppGuid(), installer::kCmdRenameChromeExe,
        ::GetCurrentProcessId(), &process_handle);
    if (FAILED(hr)) {
      TRACE_EVENT0("startup",
                   "InvokeGoogleUpdateForRename LaunchCmdElevated failed");
      LOG(ERROR) << "IProcessLauncher::LaunchCmdElevated failed; hr = "
                 << std::hex << hr;
      return false;
    }
  }

  base::Process rename_process(
      reinterpret_cast<base::ProcessHandle>(process_handle));
  int exit_code;
  {
    TRACE_EVENT0("startup", "InvokeGoogleUpdateForRename WaitForExit");
    if (!rename_process.WaitForExit(&exit_code)) {
      TRACE_EVENT0("startup", "InvokeGoogleUpdateForRename WaitForExit failed");
      PLOG(ERROR) << "WaitForExit of rename process failed";
      return false;
    }
  }

  if (exit_code != installer::RENAME_SUCCESSFUL) {
    TRACE_EVENT0("startup", "InvokeGoogleUpdateForRename !RENAME_SUCCESSFUL");
    LOG(ERROR) << "Rename process failed with exit code " << exit_code;
    return false;
  }

  TRACE_EVENT0("startup", "InvokeGoogleUpdateForRename RENAME_SUCCESSFUL");

  return true;
#else   // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return false;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace

namespace upgrade_util {

bool RelaunchChromeBrowserImpl(const base::CommandLine& command_line) {
  TRACE_EVENT0("startup", "upgrade_util::RelaunchChromeBrowserImpl");

  base::FilePath chrome_exe;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return false;
  }

  // Explicitly make sure to relaunch chrome.exe rather than old_chrome.exe.
  // This can happen when old_chrome.exe is launched by a user.
  base::CommandLine chrome_exe_command_line = command_line;
  chrome_exe_command_line.SetProgram(
      chrome_exe.DirName().Append(installer::kChromeExe));

  // Set the working directory to the exe's directory. This avoids a handle to
  // the version directory being kept open in the relaunched child process.
  base::LaunchOptions launch_options;
  launch_options.current_directory = chrome_exe.DirName();
  // Give the new process the right to bring its windows to the foreground.
  launch_options.grant_foreground_privilege = true;
  return base::LaunchProcess(chrome_exe_command_line, launch_options).IsValid();
}

bool IsUpdatePendingRestart() {
  TRACE_EVENT0("startup", "upgrade_util::IsUpdatePendingRestart");
  base::FilePath new_chrome_exe;
  if (!GetNewerChromeFile(&new_chrome_exe))
    return false;
  return base::PathExists(new_chrome_exe);
}

bool SwapNewChromeExeIfPresent() {
  if (!IsUpdatePendingRestart())
    return false;

  TRACE_EVENT0("startup", "upgrade_util::SwapNewChromeExeIfPresent");

  // If this is a system-level install, ask Google Update to launch an elevated
  // process to rename Chrome executables.
  if (install_static::IsSystemInstall())
    return InvokeGoogleUpdateForRename();

  // If this is a user-level install, directly launch a process to rename Chrome
  // executables. Obtain the command to launch the process from the registry.
  installer::AppCommand rename_cmd(installer::kCmdRenameChromeExe, {});
  if (!rename_cmd.Initialize(HKEY_CURRENT_USER))
    return false;

  base::LaunchOptions options;
  options.wait = true;
  options.start_hidden = true;
  ::SetLastError(ERROR_SUCCESS);
  base::Process process =
      base::LaunchProcess(rename_cmd.command_line(), options);
  if (!process.IsValid()) {
    PLOG(ERROR) << "Launch rename process failed";
    return false;
  }

  DWORD exit_code;
  if (!::GetExitCodeProcess(process.Handle(), &exit_code)) {
    PLOG(ERROR) << "GetExitCodeProcess of rename process failed";
    return false;
  }

  if (exit_code != installer::RENAME_SUCCESSFUL) {
    LOG(ERROR) << "Rename process failed with exit code " << exit_code;
    return false;
  }

  return true;
}

bool IsRunningOldChrome() {
  TRACE_EVENT0("startup", "upgrade_util::IsRunningOldChrome");
  // This figures out the actual file name that the section containing the
  // mapped exe refers to. This is used instead of GetModuleFileName because the
  // .exe may have been renamed out from under us while we've been running which
  // GetModuleFileName won't notice.
  wchar_t mapped_file_name[MAX_PATH * 2] = {};

  if (!::GetMappedFileName(::GetCurrentProcess(),
                           reinterpret_cast<void*>(::GetModuleHandle(NULL)),
                           mapped_file_name, std::size(mapped_file_name))) {
    return false;
  }

  base::FilePath file_name(base::FilePath(mapped_file_name).BaseName());
  return base::FilePath::CompareEqualIgnoreCase(file_name.value(),
                                                installer::kChromeOldExe);
}

bool DoUpgradeTasks(const base::CommandLine& command_line) {
  TRACE_EVENT0("startup", "upgrade_util::DoUpgradeTasks");
  // If there is no other instance already running then check if there is a
  // pending update and complete it by performing the swap and then relaunch.
  bool did_swap = false;
  if (!browser_util::IsBrowserAlreadyRunning())
    did_swap = SwapNewChromeExeIfPresent();

  // We don't need to relaunch if we didn't swap and we aren't running stale
  // binaries.
  if (!did_swap && !IsRunningOldChrome()) {
    return false;
  }

  // At this point the chrome.exe has been swapped with the new one.
  if (!RelaunchChromeBrowser(command_line)) {
    // The relaunch failed. Feel free to panic now.
    NOTREACHED();
  }
  return true;
}

}  // namespace upgrade_util

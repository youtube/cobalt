// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_LAUNCH_MODE_RECORDER_H_
#define CHROME_BROWSER_UI_STARTUP_LAUNCH_MODE_RECORDER_H_

#include "base/functional/callback_forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class CommandLine;
}

// This enum is used to define the buckets for an enumerated UMA histogram.
// Hence,
//   (a) existing enumerated constants should never be deleted or reordered,
//   and (b) new constants should only be appended at the end of the
//   enumeration.
//
// This is in the process of being deprecated. Please don't add new constants
// to it; instead, wait for the new launch mode to be added. See
// https://crbug.com/1366137.

enum class OldLaunchMode {
  kToBeDecided = 0,  // Possibly direct launch or via a shortcut.
  // Launched as an installed web application in a standalone window through any
  // method outside of a platform shortcut or command-line launch..
  // kAsWebAppInWindow = 1,     Deprecated in favor of kAsWebAppInWindowByUrl
  //                            and kAsWebAppInWindowByAppId
  kWithUrls = 2,         // Launched with urls in the cmd line.
  kOther = 3,            // Not launched from a shortcut.
  kShortcutNoName = 4,   // Launched from shortcut but no name available.
  kShortcutUnknown = 5,  // Launched from user-defined shortcut.
  // kShortcutQuickLaunch = 6,  Launched from the quick launch bar. Not used.
  //                            See kShortcutTaskbar instead.
  kShortcutDesktop = 7,  // Launched from a desktop shortcut.
  kShortcutTaskbar = 8,  // Launched from the Windows taskbar.
  kUserExperiment = 9,   // Launched after acceptance of a user experiment.
  kOtherOS = 10,         // Result bucket for OSes with no coverage here.
  kMacUndockedDiskLaunch = 11,  // Undocked launch from disk.
  kMacDockedDiskLaunch = 12,    // Docked launch from disk.
  kMacUndockedDMGLaunch = 13,   // Undocked launch from a dmg.
  kMacDockedDMGLaunch = 14,     // Docked launch from a dmg.
  kMacDockStatusError = 15,     // Error determining dock status.
  kMacDMGStatusError = 16,      // Error determining dmg status.
  kMacDockDMGStatusError = 17,  // Error determining dock and dmg status.
  // Launched from toast notification activation on Windows.
  kWinPlatformNotification = 18,
  kShortcutStartMenu = 19,  // A Windows Start Menu shortcut.
  // Started as a logon stub for the Google Credential Provider for Windows.
  kCredentialProviderSignIn = 20,
  // Launched as an installed web application in a browser tab.
  kAsWebAppInTab = 21,
  kUnknownWebApp = 22,  // The requested web application was not installed.
  kAsWebAppInWindowByUrl = 23,    // Launched the app by url with --app switch.
  kAsWebAppInWindowByAppId = 24,  // Launched app by id with --app-id switch.
  kAsWebAppInWindowOther = 25,    // Launched app by any method other than
  // through the command-line or from a platform shortcut.
  kMaxValue = kAsWebAppInWindowOther,
};

class OldLaunchModeRecorder {
 public:
  OldLaunchModeRecorder();
  OldLaunchModeRecorder(const OldLaunchModeRecorder&) = delete;
  OldLaunchModeRecorder& operator=(const OldLaunchModeRecorder&) = delete;
  ~OldLaunchModeRecorder();

  // Only sets |mode_| if it has not already been set.
  void SetLaunchMode(OldLaunchMode mode);

 private:
  absl::optional<OldLaunchMode> mode_;
};

// These enums describe how Chrome was launched. They are determined from the
// command line switches and arguments, not from what Chrome does when launched.
// "Web App" is shorthand for Chrome launched with --app or --appid switches,
// "Chrome" is Chrome launched with neither of those switches.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LaunchMode {
  kNone = 0,   // Don't record this launch.
  kOther = 1,  // Catch-all launch for Windows
  // Launched after acceptance of a user experiment.
  kUserExperiment = 2,
  kOtherOS = 3,          // Result bucket for OSes with no coverage here.
  kProtocolHandler = 4,  // Chrome launched as registered protocol handler.
  kFileTypeHandler = 5,  // Chrome launched as registered file handler.
  // Chrome launched with url(s) in the cmd line, but not from a shell
  // registration or a shortcut. Only 1st arg is considered.
  kWithUrl = 6,
  // Chrome launched with file(s) in the cmd line, but not from a shell
  // registration or a shortcut. Only first arg is considered.
  kWithFile = 7,
  // Launched from shortcut but no name available, Chrome, or Web App.
  kShortcutNoName = 8,
  // Following are shortcuts to Chrome, w/o a Web App.
  kShortcutUnknown = 9,     // Launched from user-defined shortcut.
  kShortcutDesktop = 10,    // Launched from a desktop shortcut.
  kShortcutTaskbar = 11,    // Launched from the Windows taskbar.
  kShortcutStartMenu = 12,  // A Windows Start Menu shortcut.

  // Launched from toast notification activation on Windows.
  kWinPlatformNotification = 13,
  // Launched as a logon stub for the Google Credential Provider for Windows.
  kCredentialProviderSignIn = 14,

  // Mac-specific launch modes.
  kMacUndockedDiskLaunch = 15,  // Undocked launch from disk.
  kMacDockedDiskLaunch = 16,    // Docked launch from disk.
  kMacUndockedDMGLaunch = 17,   // Undocked launch from a dmg.
  kMacDockedDMGLaunch = 18,     // Docked launch from a dmg.
  kMacDockStatusError = 19,     // Error determining dock status.
  kMacDMGStatusError = 20,      // Error determining dmg status.
  kMacDockDMGStatusError = 21,  // Error determining dock and dmg status.

  // Web App on command line, but not from a shortcut, or a file type/protocol
  // handler.
  kWebAppOther = 22,
  // Web App launched from various shortcut locations.
  kWebAppShortcutUnknown = 23,
  kWebAppShortcutDesktop = 24,
  kWebAppShortcutStartMenu = 25,
  kWebAppShortcutTaskbar = 26,
  // Web App launched as a registered file type handler.
  kWebAppFileTypeHandler = 27,
  // Web App launched as a registered protocol handler.
  kWebAppProtocolHandler = 28,
  kMaxValue = kWebAppProtocolHandler,
};

// Computes and records the launch mode based on `command_line` and process
// state. The launch mode may be recorded asynchronously.
void ComputeAndRecordLaunchMode(const base::CommandLine& command_line);

// Computes the launch mode based on `command_line` and process state. Runs
// `result_callback` with the result either synchronously or asynchronously on
// the caller's sequence.
// This is exposed for testing.
void ComputeLaunchMode(
    const base::CommandLine& command_line,
    base::OnceCallback<void(absl::optional<LaunchMode>)> result_callback);

// Returns the callback used to record launch modes. This is used by unit tests
// to verify its behavior.
base::OnceCallback<void(absl::optional<LaunchMode>)>
GetRecordLaunchModeForTesting();

#endif  // CHROME_BROWSER_UI_STARTUP_LAUNCH_MODE_RECORDER_H_

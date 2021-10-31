// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_UPDATER_UPDATER_CONSTANTS_H_
#define COBALT_UPDATER_UPDATER_CONSTANTS_H_

namespace cobalt {
namespace updater {

// Command line switches.
//
// Crash the program for testing purposes.
extern const char kCrashMeSwitch[];

// Runs as the Crashpad handler.
extern const char kCrashHandlerSwitch[];

// Installs the updater.
extern const char kInstallSwitch[];

// Uninstalls the updater.
extern const char kUninstallSwitch[];

// Updates all apps registered with the updater.
extern const char kUpdateAppsSwitch[];

// Runs in test mode. Currently, it exits right away.
extern const char kTestSwitch[];

// Disables throttling for the crash reported until the following bug is fixed:
// https://bugs.chromium.org/p/crashpad/issues/detail?id=23
extern const char kNoRateLimitSwitch[];

// The handle of an event to signal when the initialization of the main process
// is complete.
extern const char kInitDoneNotifierSwitch[];

// Enables logging.
extern const char kEnableLoggingSwitch[];

// Specifies the logging level.
extern const char kLoggingLevelSwitch[];

// Specifies the logging module filter.
extern const char kLoggingModuleSwitch[];

// URLs.
//
// Update server end point (prod).
extern const char kUpdaterJSONDefaultUrl[];
// Update server end point (qa).
extern const char kUpdaterJSONDefaultUrlQA[];

// The URL where crash reports are uploaded.
extern const char kCrashUploadURL[];
extern const char kCrashStagingUploadURL[];

// Paths.
//
// The directory name where CRX apps get installed. This is provided for demo
// purposes, since products installed by this updater will be installed in
// their specific locations.
extern const char kAppsDir[];

// The name of the uninstall script which is invoked by the --uninstall switch.
extern const char kUninstallScript[];

// Errors.
//
// The install directory for the application could not be created.
const int kCustomInstallErrorCreateAppInstallDirectory = 0;

}  // namespace updater
}  // namespace cobalt

#endif  // COBALT_UPDATER_UPDATER_CONSTANTS_H_

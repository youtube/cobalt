// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/updater_constants.h"

namespace cobalt {
namespace updater {

const char kCrashMeSwitch[] = "crash-me";
const char kCrashHandlerSwitch[] = "crash-handler";
const char kInstallSwitch[] = "install";
const char kUninstallSwitch[] = "uninstall";
const char kUpdateAppsSwitch[] = "ua";
const char kTestSwitch[] = "test";
const char kInitDoneNotifierSwitch[] = "init-done-notifier";
const char kNoRateLimitSwitch[] = "no-rate-limit";
const char kEnableLoggingSwitch[] = "enable-logging";
const char kLoggingLevelSwitch[] = "v";
const char kLoggingModuleSwitch[] = "vmodule";

const char kUpdaterJSONDefaultUrlQA[] =
    "https://omaha-qa.sandbox.google.com/service/update2/json";
const char kUpdaterJSONDefaultUrl[] =
    "https://tools.google.com/service/update2/json";
const char kCrashUploadURL[] = "https://clients2.google.com/cr/report";
const char kCrashStagingUploadURL[] =
    "https://clients2.google.com/cr/staging_report";

extern const char kAppsDir[] = "apps";
extern const char kUninstallScript[] = "uninstall.cmd";

}  // namespace updater
}  // namespace cobalt

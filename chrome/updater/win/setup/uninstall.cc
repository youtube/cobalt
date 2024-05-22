// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/win/setup/uninstall.h"

#include <windows.h>
#include <memory>

#include "base/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/win/scoped_com_initializer.h"
#include "chrome/updater/updater_constants.h"
#include "chrome/updater/util.h"
#include "chrome/updater/win/setup/setup_util.h"
#include "chrome/updater/win/task_scheduler.h"

namespace updater {

// Reverses the changes made by setup. This is a best effort uninstall:
// 1. deletes the scheduled task.
// 2. runs the uninstall script in the install directory of the updater.
// The execution of this function and the script race each other but the script
// loops and waits in between iterations trying to delete the install directory.
int Uninstall() {
  VLOG(1) << __func__;

  auto scoped_com_initializer =
      std::make_unique<base::win::ScopedCOMInitializer>(
          base::win::ScopedCOMInitializer::kMTA);

  if (!TaskScheduler::Initialize()) {
    LOG(ERROR) << "Failed to initialize the scheduler.";
    return -1;
  }
  base::ScopedClosureRunner task_scheduler_terminate_caller(
      base::BindOnce([]() { TaskScheduler::Terminate(); }));

  updater::UnregisterUpdateAppsTask();

  base::FilePath product_dir;
  if (!CreateProductDirectory(&product_dir)) {
    LOG(ERROR) << "CreateProductDirectory failed.";
    return -1;
  }

  base::char16 cmd_path[MAX_PATH] = {0};
  auto size = ExpandEnvironmentStrings(L"%SystemRoot%\\System32\\cmd.exe",
                                       cmd_path, base::size(cmd_path));
  if (!size || size >= MAX_PATH)
    return -1;

  base::FilePath script_path = product_dir.AppendASCII(kUninstallScript);

  base::string16 cmdline = cmd_path;
  base::StringAppendF(&cmdline, L" /Q /C \"%ls\"",
                      script_path.AsUTF16Unsafe().c_str());
  base::LaunchOptions options;
  options.start_hidden = true;

  VLOG(1) << "Running " << cmdline;

  auto process = base::LaunchProcess(cmdline, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to create process " << cmdline;
    return -1;
  }

  return 0;
}

}  // namespace updater

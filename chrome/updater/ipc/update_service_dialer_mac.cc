// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_dialer.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/posix_util.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

namespace {

// Start the update service by running the launcher directly.
bool DialUpdateService(const base::FilePath& updater, bool internal) {
  if (!base::PathExists(updater)) {
    // If there's no updater present, abandon dialing.
    return false;
  }
  base::CommandLine command_line(updater);
  if (internal) {
    command_line.AppendSwitch("--internal");
  }
  base::LaunchProcess(command_line, {});

  return true;
}

}  // namespace

bool DialUpdateService(UpdaterScope scope) {
  absl::optional<base::FilePath> launcher = GetUpdateServiceLauncherPath(scope);
  if (!launcher) {
    return true;
  }
  return DialUpdateService(*launcher, false);
}

bool DialUpdateInternalService(UpdaterScope scope) {
  absl::optional<base::FilePath> bundle = GetUpdaterAppBundlePath(scope);
  if (!bundle) {
    return true;
  }
  return DialUpdateService(
      bundle->Append("Contents").Append("Helpers").Append("launcher"), true);
}

}  // namespace updater

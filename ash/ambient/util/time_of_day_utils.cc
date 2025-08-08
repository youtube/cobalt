// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/util/time_of_day_utils.h"

#include <string>
#include <utility>

#include "ash/constants/ambient_time_of_day_constants.h"
#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice.pb.h"
#include "chromeos/ash/components/dbus/dlcservice/dlcservice_client.h"

namespace ash {

namespace {

void OnInstallDlcComplete(base::OnceCallback<void(base::FilePath)> on_done,
                          const DlcserviceClient::InstallResult& result) {
  CHECK(on_done);
  VLOG(1) << "Finished installing " << kTimeOfDayDlcId << " with error code "
          << result.error;
  base::FilePath install_dir;
  if (result.error == dlcservice::kErrorNone) {
    install_dir = base::FilePath(result.root_path);
  } else {
    LOG(ERROR) << "Failed to install " << kTimeOfDayDlcId << " with error "
               << result.error;
  }
  std::move(on_done).Run(install_dir);
}

void BuildAmbientVideoHtmlPath(base::OnceCallback<void(base::FilePath)> on_done,
                               base::FilePath root_dir) {
  CHECK(on_done);
  base::FilePath full_path;
  // `root_dir` can be empty if `InstallTimeOfDayDlc()` fails.
  if (!root_dir.empty()) {
    full_path = root_dir.Append(kTimeOfDayVideoHtmlSubPath);
  }
  std::move(on_done).Run(std::move(full_path));
}

// Installs the TimeOfDay DLC package containing assets for the
// Time Of Day screen saver. DLC will eventually replace the Time Of Day assets
// currently stored in rootfs. Returns the root directory where the assets are
// located. Returns an empty `base::FilePath` if the install fails.
//
// This is a successful no-op if the DLC is already installed.
void InstallTimeOfDayDlc(base::OnceCallback<void(base::FilePath)> on_done) {
  DlcserviceClient* client = DlcserviceClient::Get();
  CHECK(client);
  dlcservice::InstallRequest install_request;
  install_request.set_id(kTimeOfDayDlcId);
  VLOG(1) << "Installing " << kTimeOfDayDlcId;
  client->Install(install_request,
                  base::BindOnce(&OnInstallDlcComplete, std::move(on_done)),
                  /*ProgressCallback=*/base::DoNothing());
}

}  // namespace

void GetAmbientVideoHtmlPath(base::OnceCallback<void(base::FilePath)> on_done) {
  if (features::IsTimeOfDayDlcEnabled()) {
    InstallTimeOfDayDlc(
        base::BindOnce(&BuildAmbientVideoHtmlPath, std::move(on_done)));
  } else {
    BuildAmbientVideoHtmlPath(std::move(on_done),
                              base::FilePath(kTimeOfDayAssetsRootfsRootDir));
  }
}

const base::FilePath::CharType kTimeOfDayCloudsVideo[] =
    FILE_PATH_LITERAL("clouds.webm");
const base::FilePath::CharType kTimeOfDayNewMexicoVideo[] =
    FILE_PATH_LITERAL("new_mexico.webm");
const base::FilePath::CharType kTimeOfDayAssetsRootfsRootDir[] =
    FILE_PATH_LITERAL("/usr/share/chromeos-assets");
const base::FilePath::CharType kTimeOfDayVideoHtmlSubPath[] =
    FILE_PATH_LITERAL("personalization/time_of_day/src/ambient_video.html");

}  // namespace ash

// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/util.h"

#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cobalt/extension/installation_manager.h"
#include "components/update_client/utils.h"
#include "starboard/configuration_constants.h"
#include "starboard/loader_app/installation_manager.h"
#include "starboard/system.h"

#define PRODUCT_FULLNAME_STRING "cobalt_updater"

namespace cobalt {
namespace updater {

bool GetProductDirectory(base::FilePath* path) {
#if defined(OS_WIN)
  constexpr int kPathKey = base::DIR_LOCAL_APP_DATA;
#elif defined(OS_MACOSX)
  constexpr int kPathKey = base::DIR_APP_DATA;
#endif

#if !defined(OS_STARBOARD)
  base::FilePath app_data_dir;
  if (!base::PathService::Get(kPathKey, &app_data_dir)) {
    LOG(ERROR) << "Can't retrieve local app data directory.";
    return false;
  }
#endif

  std::vector<char> storage_dir(kSbFileMaxPath);
#if SB_API_VERSION >= 12
  if (!SbSystemGetPath(kSbSystemPathStorageDirectory, storage_dir.data(),
                       kSbFileMaxPath)) {
    SB_LOG(ERROR) << "GetProductDirectory: Failed to get "
                     "kSbSystemPathStorageDirectory";
    return false;
  }
#else
  SB_NOTREACHED() << "GetProductDirectory: kSbSystemPathStorageDirectory "
                     "is not available before "
                     "starboard version 12";
  return false;

#endif
  base::FilePath app_data_dir(storage_dir.data());
  const auto product_data_dir =
      app_data_dir.AppendASCII(PRODUCT_FULLNAME_STRING);
  if (!base::CreateDirectory(product_data_dir)) {
    LOG(ERROR) << "Can't create product directory.";
    return false;
  }

  *path = product_data_dir;
  return true;
}

const std::string GetEvergreenVersion() {
  auto installation_manager =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (!installation_manager) {
    SB_LOG(ERROR) << "Failed to get installation manager extension.";
    return "";
  }

  // Get the update version from the manifest file under the current
  // installation path.
  int index = installation_manager->GetCurrentInstallationIndex();
  if (index == IM_EXT_ERROR) {
    SB_LOG(ERROR) << "Failed to get current installation index.";
    return "";
  }
  std::vector<char> installation_path(kSbFileMaxPath);
  if (installation_manager->GetInstallationPath(index, installation_path.data(),
                                                kSbFileMaxPath) == IM_ERROR) {
    SB_LOG(ERROR) << "Failed to get installation path.";
    return "";
  }
  auto manifest = update_client::ReadManifest(base::FilePath(
      std::string(installation_path.begin(), installation_path.end())));
  if (!manifest) {
    SB_LOG(ERROR) << "Failed to read the manifest file of the current "
                     "installation.";
    return "";
  }
  auto* version_path = manifest->FindKey("version");
  if (!version_path) {
    SB_LOG(ERROR) << "Failed to find the version in the manifest.";
    return "";
  }
  return version_path->GetString();
}

}  // namespace updater
}  // namespace cobalt

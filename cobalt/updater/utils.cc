// Copyright 2021 The Cobalt Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/utils.h"

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
#include "starboard/string.h"
#include "starboard/system.h"

#define PRODUCT_FULLNAME_STRING "cobalt_updater"

namespace cobalt {
namespace updater {
namespace {
// The default manifest version to assume when the actual manifest cannot be
// parsed for any reason. This should not be used for installation manager
// errors, or any other error unrelated to parsing the manifest.
const char kDefaultManifestVersion[] = "1.0.0";
}  // namespace

bool CreateProductDirectory(base::FilePath* path) {
  if (!GetProductDirectoryPath(path)) {
    LOG(ERROR) << "Can't get product directory path";
    return false;
  }
  if (!base::CreateDirectory(*path)) {
    LOG(ERROR) << "Can't create product directory.";
    return false;
  }
  return true;
}

bool GetProductDirectoryPath(base::FilePath* path) {
#if defined(OS_WIN)
  constexpr int kPathKey = base::DIR_LOCAL_APP_DATA;
#elif defined(OS_MACOSX)
  constexpr int kPathKey = base::DIR_APP_DATA;
#endif

#if !defined(STARBOARD)
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
    SB_LOG(ERROR) << "GetProductDirectoryPath: Failed to get "
                     "kSbSystemPathStorageDirectory";
    return false;
  }
#else
  SB_NOTREACHED() << "GetProductDirectoryPath: kSbSystemPathStorageDirectory "
                     "is not available before "
                     "starboard version 12";
  return false;

#endif
  base::FilePath app_data_dir(storage_dir.data());
  const auto product_data_dir =
      app_data_dir.AppendASCII(PRODUCT_FULLNAME_STRING);

  *path = product_data_dir;
  return true;
}

base::Version ReadEvergreenVersion(base::FilePath installation_dir) {
  auto manifest = update_client::ReadManifest(installation_dir);
  if (!manifest) {
    return base::Version();
  }

  auto version = manifest->FindKey("version");
  if (version) {
    return base::Version(version->GetString());
  }
  return base::Version();
}

const std::string GetLoadedInstallationEvergreenVersion() {
  std::vector<char> system_path_content_dir(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathContentDirectory,
                       system_path_content_dir.data(), kSbFileMaxPath)) {
    SB_LOG(ERROR) << "Failed to get system path content directory";
    return "";
  }
  // Get the parent directory of the system_path_content_dir, and read the
  // manifest.json there
  base::Version version = ReadEvergreenVersion(
      base::FilePath(std::string(system_path_content_dir.begin(),
                                 system_path_content_dir.end()))
          .DirName());

  if (!version.IsValid()) {
    SB_LOG(ERROR) << "Failed to get the Evergreen version. Defaulting to "
                  << kDefaultManifestVersion << ".";
    return std::string(kDefaultManifestVersion);
  }
  return version.GetString();
}

const std::string GetCurrentEvergreenVersion() {
  auto installation_manager =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (!installation_manager) {
    SB_LOG(ERROR) << "Failed to get installation manager extension, getting "
                     "the Evergreen version of the loaded installation.";
    return GetLoadedInstallationEvergreenVersion();
  }
  // Get the update version from the manifest file under the current
  // installation path.
  int index = installation_manager->GetCurrentInstallationIndex();
  if (index == IM_EXT_ERROR) {
    SB_LOG(ERROR) << "Failed to get current installation index, getting the "
                     "Evergreen version of the currently loaded installation.";
    return GetLoadedInstallationEvergreenVersion();
  }
  std::vector<char> installation_path(kSbFileMaxPath);
  if (installation_manager->GetInstallationPath(
          index, installation_path.data(), kSbFileMaxPath) == IM_EXT_ERROR) {
    SB_LOG(ERROR) << "Failed to get installation path, getting the Evergreen "
                     "version of the currently loaded installation.";
    return GetLoadedInstallationEvergreenVersion();
  }

  base::Version version = ReadEvergreenVersion(base::FilePath(
      std::string(installation_path.begin(), installation_path.end())));

  if (!version.IsValid()) {
    SB_LOG(ERROR) << "Failed to get the Evergreen version. Defaulting to "
                  << kDefaultManifestVersion << ".";
    return std::string(kDefaultManifestVersion);
  }
  return version.GetString();
}

}  // namespace updater
}  // namespace cobalt

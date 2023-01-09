// Copyright 2021 The Cobalt Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cobalt/updater/utils.h"

#include <memory>
#include <vector>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/update_client/utils.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "starboard/configuration_constants.h"
#include "starboard/extension/installation_manager.h"
#include "starboard/file.h"
#include "starboard/string.h"
#include "starboard/system.h"

#define PRODUCT_FULLNAME_STRING "cobalt_updater"

namespace cobalt {
namespace updater {
namespace {
// Path to compressed Cobalt library, relative to the installation directory.
const char kCompressedLibraryPath[] = "lib/libcobalt.lz4";

// Path to uncompressed Cobalt library, relative to the installation directory.
const char kUncompressedLibraryPath[] = "lib/libcobalt.so";

}  // namespace

const char kDefaultManifestVersion[] = "1.0.0";

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
  if (!SbSystemGetPath(kSbSystemPathStorageDirectory, storage_dir.data(),
                       kSbFileMaxPath)) {
    LOG(ERROR) << "GetProductDirectoryPath: Failed to get "
                  "kSbSystemPathStorageDirectory";
    return false;
  }
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

const std::string GetEvergreenFileType(const std::string& installation_path) {
  std::string compressed_library_path = base::StrCat(
      {installation_path, kSbFileSepString, kCompressedLibraryPath});
  std::string uncompressed_library_path = base::StrCat(
      {installation_path, kSbFileSepString, kUncompressedLibraryPath});

  if (SbFileExists(compressed_library_path.c_str())) {
    return "Compressed";
  } else if (SbFileExists(uncompressed_library_path.c_str())) {
    return "Uncompressed";
  } else {
    LOG(ERROR) << "Failed to get Evergreen file type. Defaulting to "
                  "FileTypeUnknown.";
    return "FileTypeUnknown";
  }
}

const base::FilePath GetLoadedInstallationPath() {
  std::vector<char> system_path_content_dir(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathContentDirectory,
                       system_path_content_dir.data(), kSbFileMaxPath)) {
    LOG(ERROR) << "Failed to get system path content directory";
    return base::FilePath();
  }
  // Since the Cobalt library has already been loaded,
  // kSbSystemPathContentDirectory points to the content dir of the running
  // library and the installation dir is therefore its parent.
  return base::FilePath(std::string(system_path_content_dir.begin(),
                                    system_path_content_dir.end()))
      .DirName();
}

const base::FilePath FindInstallationPath(
    std::function<const void*(const char*)> get_extension_fn) {
  // TODO(b/233914266): consider using base::NoDestructor to give the
  // installation path static duration once found.

  auto installation_manager =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          get_extension_fn(kCobaltExtensionInstallationManagerName));
  if (!installation_manager) {
    LOG(ERROR) << "Failed to get installation manager extension, getting the "
                  "installation path of the loaded library.";
    return GetLoadedInstallationPath();
  }
  // Get the update version from the manifest file under the current
  // installation path.
  int index = installation_manager->GetCurrentInstallationIndex();
  if (index == IM_EXT_ERROR) {
    LOG(ERROR) << "Failed to get current installation index, getting the "
                  "installation path of the loaded library.";
    return GetLoadedInstallationPath();
  }
  std::vector<char> installation_path(kSbFileMaxPath);
  if (installation_manager->GetInstallationPath(
          index, installation_path.data(), kSbFileMaxPath) == IM_EXT_ERROR) {
    LOG(ERROR) << "Failed to get installation path from the installation "
                  "manager, getting the installation path of the loaded "
                  "library.";
    return GetLoadedInstallationPath();
  }
  return base::FilePath(
      std::string(installation_path.begin(), installation_path.end()));
}

const std::string GetValidOrDefaultEvergreenVersion(
    const base::FilePath installation_path) {
  base::Version version = ReadEvergreenVersion(installation_path);

  if (!version.IsValid()) {
    LOG(ERROR) << "Failed to get the Evergreen version. Defaulting to "
               << kDefaultManifestVersion << ".";
    return std::string(kDefaultManifestVersion);
  }
  return version.GetString();
}

const std::string GetCurrentEvergreenVersion(
    std::function<const void*(const char*)> get_extension_fn) {
  base::FilePath installation_path = FindInstallationPath(get_extension_fn);
  return GetValidOrDefaultEvergreenVersion(installation_path);
}

EvergreenLibraryMetadata GetCurrentEvergreenLibraryMetadata(
    std::function<const void*(const char*)> get_extension_fn) {
  EvergreenLibraryMetadata evergreen_library_metadata;
  base::FilePath installation_path = FindInstallationPath(get_extension_fn);

  evergreen_library_metadata.version =
      GetValidOrDefaultEvergreenVersion(installation_path);
  evergreen_library_metadata.file_type =
      GetEvergreenFileType(installation_path.value());

  return evergreen_library_metadata;
}

std::string GetLibrarySha256(int index) {
  base::FilePath filepath;
  auto installation_manager =
      static_cast<const CobaltExtensionInstallationManagerApi*>(
          SbSystemGetExtension(kCobaltExtensionInstallationManagerName));
  if (!installation_manager && index == 0) {
    // Evergreen Lite
    std::vector<char> system_path_content_dir(kSbFileMaxPath);
    if (!SbSystemGetPath(kSbSystemPathContentDirectory,
                         system_path_content_dir.data(), kSbFileMaxPath)) {
      LOG(ERROR)
          << "GetLibrarySha256: Failed to get system path content directory";
      return "";
    }

    filepath = base::FilePath(std::string(system_path_content_dir.begin(),
                                          system_path_content_dir.end()))
                   .DirName();
  } else if (!installation_manager && index != 0) {
    LOG(ERROR) << "GetLibrarySha256: Evergreen lite supports only slot 0";
    return "";
  } else {
    // Evergreen Full
    std::vector<char> installation_path(kSbFileMaxPath);
    if (installation_manager->GetInstallationPath(
            index, installation_path.data(), kSbFileMaxPath) == IM_EXT_ERROR) {
      LOG(ERROR) << "GetLibrarySha256: Failed to get installation path";
      return "";
    }

    filepath = base::FilePath(installation_path.data());
  }

  filepath = filepath.AppendASCII("lib").AppendASCII("libcobalt.so");
  base::File source_file(filepath,
                         base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!source_file.IsValid()) {
    LOG(ERROR) << "GetLibrarySha256(): Unable to open source file: "
               << filepath.value();
    return "";
  }

  const size_t kBufferSize = 32768;
  std::vector<char> buffer(kBufferSize);
  uint8_t actual_hash[crypto::kSHA256Length] = {0};
  std::unique_ptr<crypto::SecureHash> hasher(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  while (true) {
    int bytes_read = source_file.ReadAtCurrentPos(&buffer[0], buffer.size());
    if (bytes_read < 0) {
      LOG(ERROR) << "GetLibrarySha256(): error reading from: "
                 << filepath.value();

      return "";
    }

    if (bytes_read == 0) {
      break;
    }

    hasher->Update(&buffer[0], bytes_read);
  }

  hasher->Finish(actual_hash, sizeof(actual_hash));

  return base::HexEncode(actual_hash, sizeof(actual_hash));
}

}  // namespace updater
}  // namespace cobalt

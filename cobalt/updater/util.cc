// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/updater/util.h"

#include <sys/stat.h>

#include <memory>
#include <vector>

#include "base/base_paths.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/update_client/utils.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "starboard/configuration_constants.h"
#include "starboard/extension/installation_manager.h"
#include "starboard/file.h"
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

const auto kChannelAndSbVersionToOmahaIdMap = base::
    MakeFixedFlatMap<base::StringPiece, base::StringPiece, kOmahaIdMapSize>({
        {"control17", "{40061C09-D926-4B82-8D42-600C06B6134C}"},
        {"dogfood17", "{E91075F4-353A-4E7D-A339-6563D2B7858D}"},
        {"experiment17", "{42B5CF5A-96A4-4F64-AD0E-7C62705222FF}"},
        {"prod17", "{20F11416-0D9C-4CB1-A82A-0168594E8256}"},
        {"qa17", "{C725A22D-553A-49DC-BD61-F042B07C6B22}"},
        {"rollback17", "{3A1FCBE4-E4F9-4DC3-9E1A-700FBC1D551B}"},
        {"static17", "{B68BE0E4-7EE3-451A-9395-A789518FF7C5}"},
        {"t1app17", "{C677F645-A8DB-4014-BD95-C6C06715C7CE}"},
        {"tcrash17", "{1F876BD6-7C15-4B09-8C95-9FBBA2F93E94}"},
        {"test17", "{6EADD81E-A98E-4F8B-BFAA-875509A51991}"},
        {"tfailv17", "{9F9EC6E9-B89D-4C75-9E7E-A5B9B0254844}"},
        {"tmsabi17", "{2B915523-8ADD-4C66-9E8F-D73FB48A4296}"},
        {"tnoop17", "{6F4E8AD9-067B-443A-8B63-A7CC4C95B264}"},
        {"tseries117", "{7E7C6582-3DC4-4B48-97F2-FA43614B2B4D}"},
        {"tseries217", "{112BF4F5-8463-490F-B6C8-E9B64D972152}"},
    });

const char kDefaultManifestVersion[] = "1.0.0";

const char kOmahaCobaltEAPAppID[] = "{E79B0BFB-BF43-4705-B15F-E6B9B7E5136B}";
const char kOmahaCobaltLTSNightlyAppID[] =
    "{26CD2F67-091F-4680-A9A9-2229635B65A5}";
const char kOmahaCobaltTrunkAppID[] = "{A9557415-DDCD-4948-8113-C643EFCF710C}";

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
  // This is necessary to allow blocking for reading Evergreen version and
  // populating it in the user agent string to prevent
  // a crash due to blocking restrictions.
  // TODO(b/468067703): Investigate async approach with task posting.
  base::ScopedAllowBlocking allow_blocking;
  auto manifest = update_client::ReadManifest(installation_dir);
  if (manifest) {
    auto version = manifest->Find("version");
    if (version) {
      return base::Version(version->GetString());
    }
  }
  return base::Version();
}

const std::string GetEvergreenFileType(const std::string& installation_path) {
  std::string compressed_library_path = base::StrCat(
      {installation_path, kSbFileSepString, kCompressedLibraryPath});
  std::string uncompressed_library_path = base::StrCat(
      {installation_path, kSbFileSepString, kUncompressedLibraryPath});
  struct stat file_info;
  if (stat(compressed_library_path.c_str(), &file_info) == 0) {
    return "Compressed";
  }
  if (stat(uncompressed_library_path.c_str(), &file_info) == 0) {
    return "Uncompressed";
  }
  LOG(ERROR) << "Failed to get Evergreen file type. Defaulting to "
                "FileTypeUnknown.";
  return "FileTypeUnknown";
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
  // TODO(b/452143961): Fix the discrepancy between char and uint8_t
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

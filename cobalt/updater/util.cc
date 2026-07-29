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
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/update_client/utils.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "starboard/configuration_constants.h"
#include "starboard/extension/installation_manager.h"
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

const std::unordered_map<std::string, std::string>
    kChannelAndSbVersionToOmahaIdMap = {
        {"control18", "{18C5B2A4-D8E2-4F31-9A4C-7E2B1D4F8A31}"},
        {"dogfood18", "{29D6C3B5-E9F3-4042-AB5D-8F3C2E5A9B42}"},
        {"experiment18", "{4BF8E5D7-0B15-4264-CD7F-A15E407CBD64}"},
        {"prod18", "{5C09F6E8-1C26-4375-DE80-B26F518DCE75}"},
        {"qa18", "{6D1A07F9-2D37-4486-EF91-C370629EDF86}"},
        {"rollback18", "{7E2B180A-3E48-4597-F0A2-D48173AFE097}"},
        {"static18", "{8F3C291B-4F59-46A8-01B3-E59284B0F1A8}"},
        {"t1app18", "{904D3A2C-506A-47B9-12C4-F6A395C102B9}"},
        {"tcrash18", "{A15E4B3D-617B-48CA-23D5-07B4A6D213CA}"},
        {"test18", "{B26F5C4E-728C-49DB-34E6-18C5B7E324DB}"},
        {"tfailv18", "{C3706D5F-839D-4AEC-45F7-29D6C8F435EC}"},
        {"tmsabi18", "{D4817E60-94AE-4BFD-5608-3AE7D90546FD}"},
        {"tnoop18", "{E5928F71-A5BF-4C0E-6719-4BF8EA16570E}"},
        {"tseries118", "{F6A39082-B6C0-4D1F-782A-5C09FB27681F}"},
        {"tseries218", "{07B4A193-C7D1-4E20-893B-6D1A0C387920}"},
};

const char kDefaultManifestVersion[] = "1.0.0";

const char kOmahaCobalt27NightlyAppID[] =
    "{7B255C60-876C-41E1-A5E7-C0F9EBE78772}";
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

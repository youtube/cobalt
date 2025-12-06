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

const std::unordered_map<std::string, std::string>
    kChannelAndSbVersionToOmahaIdMap = {
        {"control14", "{5F96FC23-F232-40B6-B283-FAD8DB21E1A7}"},
        {"control15", "{409F15C9-4E10-4224-9DD1-BEE7E5A2A55B}"},
        {"control16", "{30061C09-D926-4B82-8D42-600C06B6134C}"},
        {"experiment14", "{9BCC272B-3F78-43FD-9B34-A3810AE1B85D}"},
        {"experiment15", "{C412A665-9BAD-4981-93C0-264233720222}"},
        {"experiment16", "{32B5CF5A-96A4-4F64-AD0E-7C62705222FF}"},
        {"prod14", "{B3F9BCA2-8AD1-448F-9829-BB9F432815DE}"},
        {"prod15", "{14ED1D09-DAD2-4FB2-A89B-3ADC82137BCD}"},
        {"prod16", "{10F11416-0D9C-4CB1-A82A-0168594E8256}"},
        {"qa14", "{94C5D27F-5981-46E8-BD4F-4645DBB5AFD3}"},
        {"qa15", "{14ED1D09-DAD2-4FB2-A89B-3ADC82137BCD}"},
        {"qa16", "{B725A22D-553A-49DC-BD61-F042B07C6B22}"},
        {"rollback14", "{A83768A9-9556-48C2-8D5E-D7C845C16C19}"},
        {"rollback15", "{1526831E-B130-43C1-B31A-AEE6E90063ED}"},
        {"rollback16", "{2A1FCBE4-E4F9-4DC3-9E1A-700FBC1D551B}"},
        {"static14", "{8001FE2C-F523-4AEC-A753-887B5CC35EBB}"},
        {"static15", "{F58B7C5F-8DC5-4E32-8CF1-455F1302D609}"},
        {"static16", "{A68BE0E4-7EE3-451A-9395-A789518FF7C5}"},
        {"t1app14", "{7C8BCB72-705D-41A6-8189-D8312E795302}"},
        {"t1app15", "{12A94004-F661-42A7-9DFC-325A4DA72D29}"},
        {"t1app16", "{B677F645-A8DB-4014-BD95-C6C06715C7CE}"},
        {"tcrash14", "{328C380E-75B4-4D7A-BF98-9B443ABA8FFF}"},
        {"tcrash15", "{1A924F1C-A46D-4104-8CCE-E8DB3C6E0ED1}"},
        {"tcrash16", "{0F876BD6-7C15-4B09-8C95-9FBBA2F93E94}"},
        {"test14", "{DB2CC00C-4FA9-4DB8-A947-647CEDAEBF29}"},
        {"test15", "{24A8A3BF-5944-4D5C-A5C4-BE00DF0FB9E1}"},
        {"test16", "{5EADD81E-A98E-4F8B-BFAA-875509A51991}"},
        {"tfailv14", "{F06C3516-8706-4579-9493-881F84606E98}"},
        {"tfailv15", "{36CFD9A7-1A73-4E8A-AC05-E3013CC4F75C}"},
        {"tfailv16", "{8F9EC6E9-B89D-4C75-9E7E-A5B9B0254844}"},
        {"tmsabi14", "{87EDA0A7-ED13-4A72-9CD9-EBD4BED081AB}"},
        {"tmsabi15", "{60296D57-2572-4B16-89EC-C9DA5A558E8A}"},
        {"tmsabi16", "{1B915523-8ADD-4C66-9E8F-D73FB48A4296}"},
        {"tnoop14", "{C407CF3F-21A4-47AA-9667-63E7EEA750CB}"},
        {"tnoop15", "{FC568D82-E608-4F15-95F0-A539AB3F6E9D}"},
        {"tnoop16", "{5F4E8AD9-067B-443A-8B63-A7CC4C95B264}"},
        {"tseries114", "{0A8A3F51-3FAB-4427-848E-28590DB75AA1}"},
        {"tseries115", "{92B7AC78-1B3B-4CE6-BA39-E1F50C4F5F72}"},
        {"tseries116", "{6E7C6582-3DC4-4B48-97F2-FA43614B2B4D}"},
        {"tseries214", "{7CB65840-5FA4-4706-BC9E-86A89A56B4E0}"},
        {"tseries215", "{7CCA7DB3-C27D-4CEB-B6A5-50A4D6DE40DA}"},
        {"tseries216", "{012BF4F5-8463-490F-B6C8-E9B64D972152}"},
};

const char kDefaultManifestVersion[] = "1.0.0";

const char kOmahaCobaltAppID[] = "{6D4E53F3-CC64-4CB8-B6BD-AB0B8F300E1C}";
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
  std::string storage_dir(kSbFileMaxPath, 0);
  if (!SbSystemGetPath(kSbSystemPathStorageDirectory, &storage_dir[0],
                       kSbFileMaxPath)) {
    LOG(ERROR) << "GetProductDirectoryPath: Failed to get "
                  "kSbSystemPathStorageDirectory";
    return false;
  }
  storage_dir.resize(strlen(storage_dir.c_str()));
  base::FilePath app_data_dir(storage_dir);
  const auto product_data_dir =
      app_data_dir.AppendASCII(PRODUCT_FULLNAME_STRING);

  *path = product_data_dir;
  return true;
}

base::Version ReadEvergreenVersion(base::FilePath installation_dir) {
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
  std::string system_path_content_dir(kSbFileMaxPath, 0);
  if (!SbSystemGetPath(kSbSystemPathContentDirectory,
                       &system_path_content_dir[0], kSbFileMaxPath)) {
    LOG(ERROR) << "Failed to get system path content directory";
    return base::FilePath();
  }
  system_path_content_dir.resize(strlen(system_path_content_dir.c_str()));
  // Since the Cobalt library has already been loaded,
  // kSbSystemPathContentDirectory points to the content dir of the running
  // library and the installation dir is therefore its parent.
  return base::FilePath(system_path_content_dir).DirName();
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
    std::string system_path_content_dir(kSbFileMaxPath, 0);
    if (!SbSystemGetPath(kSbSystemPathContentDirectory,
                         &system_path_content_dir[0], kSbFileMaxPath)) {
      LOG(ERROR)
          << "GetLibrarySha256: Failed to get system path content directory";
      return "";
    }
    system_path_content_dir.resize(strlen(system_path_content_dir.c_str()));

    filepath = base::FilePath(system_path_content_dir).DirName();
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

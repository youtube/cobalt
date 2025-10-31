// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/slot_management.h"

#include <stdio.h>
#include <sys/stat.h>

#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "starboard/common/check_op.h"
#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/crashpad_wrapper/annotations.h"
#include "starboard/crashpad_wrapper/wrapper.h"
#include "starboard/elf_loader/elf_loader_constants.h"
#include "starboard/elf_loader/sabi_string.h"
#include "starboard/event.h"
#include "starboard/extension/loader_app_metrics.h"
#include "starboard/loader_app/app_key_files.h"
#include "starboard/loader_app/drain_file.h"
#include "starboard/loader_app/installation_manager.h"
#include "third_party/jsoncpp/source/include/json/reader.h"
#include "third_party/jsoncpp/source/include/json/value.h"

namespace loader_app {
namespace {

// The max length of Evergreen version string.
const int kMaxEgVersionLength = 20;

// The max number of installations slots.
const int kMaxNumInstallations = 2;

// Relative path for the directory of the Cobalt shared library.
const char kCobaltLibraryPath[] = "lib";

// Filename for the Cobalt binary.
const char kCobaltLibraryName[] = "libcobalt.so";

// Filename for the compressed Cobalt binary.
const char kCompressedCobaltLibraryName[] = "libcobalt.lz4";

// Relative path for the content directory of
// the Cobalt installation.
const char kCobaltContentPath[] = "content";

// Filename for the manifest file which contains the Evergreen version.
const char kManifestFileName[] = "manifest.json";

// Deliminator of the Evergreen version string segments.
const char kEgVersionDeliminator = '.';

// Evergreen version key in the manifest file.
const char kVersionKey[] = "version";

}  // namespace

// Compares the Evergreen versions v1 and v2. Returns 1 if v1 is newer than v2;
// returns -1 if v1 is older than v2; returns 0 if v1 is the same as v2, or if
// either of them is invalid.
int CompareEvergreenVersion(std::vector<char>* v1, std::vector<char>* v2) {
  if ((*v1)[0] == '\0' || (*v2)[0] == '\0') {
    return 0;
  }

  // Split the version strings into segments of numbers
  std::vector<int> n1, n2;
  std::stringstream s1(std::string(v1->begin(), v1->end()));
  std::stringstream s2(std::string(v2->begin(), v2->end()));
  std::string seg;
  while (std::getline(s1, seg, kEgVersionDeliminator)) {
    n1.push_back(std::stoi(seg));
  }
  while (std::getline(s2, seg, kEgVersionDeliminator)) {
    n2.push_back(std::stoi(seg));
  }

  // Compare each segment
  int size = std::min(n1.size(), n2.size());
  for (int i = 0; i < size; i++) {
    if (n1[i] > n2[i]) {
      return 1;
    } else if (n1[i] < n2[i]) {
      return -1;
    }
  }

  // If all segments are equal, compare the lengths
  if (n1.size() > n2.size()) {
    return 1;
  } else if (n1.size() < n2.size()) {
    return -1;
  }
  return 0;
}

// Reads the Evergreen version from the manifest file at the
// |manifest_file_path|, and stores in |version|.
bool ReadEvergreenVersion(std::vector<char>* manifest_file_path,
                          char* version,
                          int version_length) {
  // Check the manifest file exists
  struct stat info;
  if (stat(manifest_file_path->data(), &info) != 0) {
    SB_LOG(WARNING)
        << "Failed to open the manifest file at the installation path.";
    return false;
  }

  starboard::ScopedFile manifest_file(manifest_file_path->data(), O_RDONLY,
                                      S_IRWXU | S_IRGRP);
  int64_t file_size = manifest_file.GetSize();
  std::vector<char> file_data(file_size);
  int read_size = manifest_file.ReadAll(file_data.data(), file_size);
  if (read_size < 0) {
    SB_LOG(WARNING) << "Error while reading from the manifest file.";
    return false;
  }

  Json::Reader reader;
  Json::Value obj;
  if (!reader.parse(std::string(file_data.data(), file_size), obj) ||
      !obj[kVersionKey]) {
    SB_LOG(WARNING) << "Failed to parse version from the manifest file at the "
                       "installation path.";
    return false;
  }

  snprintf(version, version_length, "%s", obj[kVersionKey].asString().c_str());
  return true;
}

bool GetEvergreenVersionByIndex(int installation_index,
                                char* version,
                                int version_length) {
  std::vector<char> installation_path(kSbFileMaxPath);
  if (ImGetInstallationPath(installation_index, installation_path.data(),
                            kSbFileMaxPath) == IM_ERROR) {
    SB_LOG(ERROR) << "Failed to get installation path of installation index "
                  << installation_index;
    return false;
  }
  std::vector<char> manifest_file_path(kSbFileMaxPath);
  snprintf(manifest_file_path.data(), kSbFileMaxPath, "%s%s%s",
           installation_path.data(), kSbFileSepString, kManifestFileName);
  if (!ReadEvergreenVersion(&manifest_file_path, version, version_length)) {
    SB_LOG(WARNING)
        << "Failed to read the Evergreen version of installation index "
        << installation_index;
    return false;
  }
  return true;
}

int RevertBack(int current_installation,
               const std::string& app_key,
               bool mark_bad,
               SlotSelectionStatus status) {
  SB_LOG(INFO) << "RevertBack current_installation=" << current_installation;
  SB_DCHECK_NE(current_installation, 0);
  if (mark_bad) {
    std::vector<char> installation_path(kSbFileMaxPath);
    if (ImGetInstallationPath(current_installation, installation_path.data(),
                              kSbFileMaxPath) != IM_ERROR) {
      std::string bad_app_key_file_path =
          loader_app::GetBadAppKeyFilePath(installation_path.data(), app_key);
      if (bad_app_key_file_path.empty()) {
        SB_LOG(WARNING) << "Failed to get bad app key file path for path="
                        << installation_path.data()
                        << " and app_key=" << app_key;
      } else {
        if (!loader_app::CreateAppKeyFile(bad_app_key_file_path)) {
          SB_LOG(WARNING) << "Failed to create bad app key file: "
                          << bad_app_key_file_path;
        }
      }
    } else {
      SB_LOG(WARNING) << "Failed to get installation path for index: "
                      << current_installation;
    }
  }
  current_installation = ImRevertToSuccessfulInstallation(status);
  return current_installation;
}

bool CheckBadFileExists(const char* installation_path, const char* app_key) {
  std::string bad_app_key_file_path =
      loader_app::GetBadAppKeyFilePath(installation_path, app_key);
  SB_DCHECK(!bad_app_key_file_path.empty());
  struct stat info;
  bool file_exists = stat(bad_app_key_file_path.c_str(), &info) == 0;
  SB_LOG(INFO) << "bad_app_key_file_path: " << bad_app_key_file_path;
  SB_LOG(INFO) << "bad_app_key_file_path FileExists: " << file_exists;
  return !bad_app_key_file_path.empty() && file_exists;
}

bool AdoptInstallation(int current_installation,
                       const char* installation_path,
                       const char* app_key) {
  SB_LOG(INFO) << "AdoptInstallation: current_installation="
               << current_installation
               << " installation_path=" << installation_path
               << " app_key=" << app_key;
  // Check that a good file exists from at least one app before adopting.
  if (!AnyGoodAppKeyFile(installation_path)) {
    SB_LOG(ERROR) << "No good files present";
    return false;
  }
  std::string good_app_key_file_path =
      loader_app::GetGoodAppKeyFilePath(installation_path, app_key);
  if (good_app_key_file_path.empty()) {
    SB_LOG(WARNING) << "Failed to get good app key file path for app_key="
                    << app_key;
    return false;
  }
  struct stat info;
  if (stat(good_app_key_file_path.c_str(), &info) != 0) {
    if (!loader_app::CreateAppKeyFile(good_app_key_file_path)) {
      SB_LOG(WARNING) << "Failed to create good app key file";
      return false;
    }
    if (ImResetInstallation(current_installation) == IM_ERROR) {
      return false;
    }
    if (ImRollForward(current_installation) == IM_ERROR) {
      SB_LOG(WARNING) << "Failed to roll forward";
      return false;
    }
  }
  return true;
}

void* LoadSlotManagedLibrary(const std::string& app_key,
                             const std::string& alternative_content_path,
                             LibraryLoader* library_loader,
                             bool use_memory_mapped_file) {
  // Initialize the Installation Manager.
  if (ImInitialize(kMaxNumInstallations, app_key.c_str()) != IM_SUCCESS) {
    SB_LOG(ERROR) << "Abort. Failed to initialize Installation Manager";
    return NULL;
  }

  // Roll forward if needed.
  if (ImRollForwardIfNeeded() == IM_ERROR) {
    SB_LOG(WARNING) << "Failed to roll forward";
  }

  int current_installation = ImGetCurrentInstallationIndex();

  // Check the system image. If it's newer than the current slot, update to
  // system image immediately.
  if (current_installation != 0) {
    std::vector<char> current_version(kMaxEgVersionLength);
    if (!GetEvergreenVersionByIndex(current_installation,
                                    current_version.data(),
                                    kMaxEgVersionLength)) {
      SB_LOG(WARNING)
          << "Failed to get the Evergreen version of installation index "
          << current_installation;
    }

    std::vector<char> system_image_version(kMaxEgVersionLength);
    if (!GetEvergreenVersionByIndex(0, system_image_version.data(),
                                    kMaxEgVersionLength)) {
      SB_LOG(WARNING)
          << "Failed to get the Evergreen version of installation index " << 0;
    }

    if (CompareEvergreenVersion(&system_image_version, &current_version) > 0) {
      if (ImRollForward(0) != IM_ERROR) {
        current_installation = 0;
      } else {
        SB_LOG(WARNING) << "Failed to roll forward to system image";
      }
    }
  }

  // TODO: Try to simplify the loop.
  // Loop by priority.
  while (current_installation != IM_ERROR) {
    // if not successful and num_tries_left > 0 decrement and try to
    // load the library.
    if (ImGetInstallationStatus(current_installation) !=
        IM_INSTALLATION_STATUS_SUCCESS) {
      int num_tries_left = ImGetInstallationNumTriesLeft(current_installation);
      if (num_tries_left == IM_ERROR || num_tries_left <= 0 ||
          ImDecrementInstallationNumTries(current_installation) == IM_ERROR) {
        SB_LOG(INFO) << "Out of retries";
        // If no more tries are left or if we have hard failure,
        // discard the image and auto rollback, but only if
        // the current image is not the system image.
        if (current_installation != 0) {
          current_installation =
              RevertBack(current_installation, app_key, true /* mark_bad */,
                         SlotSelectionStatus::kRollBackOutOfRetries);
        }
      }
    }

    SB_LOG(INFO) << "Try to load the Cobalt binary";
    SB_LOG(INFO) << "current_installation=" << current_installation;

    //  Try to load the image. Failures here discard the image.
    std::vector<char> installation_path(kSbFileMaxPath);
    if (ImGetInstallationPath(current_installation, installation_path.data(),
                              kSbFileMaxPath) == IM_ERROR) {
      SB_LOG(ERROR) << "Failed to find library file";

      // Hard failure. Discard the image and auto rollback, but only if
      // the current image is not the system image.
      if (current_installation != 0) {
        current_installation =
            RevertBack(current_installation, app_key, true /* mark_bad */,
                       SlotSelectionStatus::kRollBackNoLibFile);
        continue;
      } else {
        // The system image at index 0 failed.
        return NULL;
      }
    }

    SB_DLOG(INFO) << "installation_path=" << installation_path.data();

    if (current_installation != 0) {
      // Cleanup all expired files from all apps.
      DrainFileClearExpired(installation_path.data());

      // Cleanup all drain files from the current app.
      DrainFileClearForApp(installation_path.data(), app_key.c_str());

      // Check for bad file.
      if (CheckBadFileExists(installation_path.data(), app_key.c_str())) {
        SB_LOG(INFO) << "Bad app key file";
        current_installation =
            RevertBack(current_installation, app_key, true /* mark_bad */,
                       SlotSelectionStatus::kRollBackBadAppKeyFile);
        continue;
      }
      // If the current installation is in use by an updater roll back.
      if (DrainFileIsAnotherAppDraining(installation_path.data(),
                                        app_key.c_str())) {
        SB_LOG(INFO) << "Active slot draining";
        current_installation =
            RevertBack(current_installation, app_key, false /* mark_bad */,
                       SlotSelectionStatus::kRollBackSlotDraining);
        continue;
      }
      // Adopt installation performed from different app.
      if (!AdoptInstallation(current_installation, installation_path.data(),
                             app_key.c_str())) {
        SB_LOG(INFO) << "Unable to adopt installation";
        current_installation =
            RevertBack(current_installation, app_key, true /* mark_bad */,
                       SlotSelectionStatus::kRollBackFailedToAdopt);
        continue;
      }
    }

    // installation_n/lib/libcobalt.so
    std::vector<char> compressed_lib_path(kSbFileMaxPath);
    snprintf(compressed_lib_path.data(), kSbFileMaxPath, "%s%s%s%s%s",
             installation_path.data(), kSbFileSepString, kCobaltLibraryPath,
             kSbFileSepString, kCompressedCobaltLibraryName);
    std::vector<char> uncompressed_lib_path(kSbFileMaxPath);
    snprintf(uncompressed_lib_path.data(), kSbFileMaxPath, "%s%s%s%s%s",
             installation_path.data(), kSbFileSepString, kCobaltLibraryPath,
             kSbFileSepString, kCobaltLibraryName);

    std::string lib_path;
    bool use_compression;
    struct stat info;
    if (stat(compressed_lib_path.data(), &info) == 0) {
      lib_path = compressed_lib_path.data();
      use_compression = true;
    } else if (stat(uncompressed_lib_path.data(), &info) == 0) {
      lib_path = uncompressed_lib_path.data();
      use_compression = false;
    } else {
      SB_LOG(ERROR) << "No library found at compressed "
                    << compressed_lib_path.data() << " or uncompressed "
                    << uncompressed_lib_path.data() << " path";
      return NULL;
    }

    if (use_compression && use_memory_mapped_file) {
      SB_LOG(ERROR) << "Using both compression and mmap files is not supported";
      return NULL;
    }

    SB_LOG(INFO) << "lib_path=" << lib_path;

    std::string content;
    if (alternative_content_path.empty()) {
      // installation_n/content
      std::vector<char> content_path(kSbFileMaxPath);
      snprintf(content_path.data(), kSbFileMaxPath, "%s%s%s",
               installation_path.data(), kSbFileSepString, kCobaltContentPath);
      content = content_path.data();
    } else {
      content = alternative_content_path.c_str();
    }

    SB_LOG(INFO) << "content=" << content;

    if (!library_loader->Load(lib_path, content.c_str(), use_compression,
                              use_memory_mapped_file)) {
      SB_LOG(WARNING) << "Failed to load Cobalt!";

      // Hard failure. Discard the image and auto rollback, but only if
      // the current image is not the system image.
      if (current_installation != 0) {
        current_installation =
            RevertBack(current_installation, app_key, true /* mark_bad */,
                       SlotSelectionStatus::kRollBackFailedToLoadCobalt);
        continue;
      } else {
        // The system image at index 0 failed.
        return NULL;
      }
    }

    EvergreenInfo evergreen_info;
    GetEvergreenInfo(&evergreen_info);
    if (!crashpad::AddEvergreenInfoToCrashpad(evergreen_info)) {
      SB_LOG(ERROR)
          << "Could not send Cobalt library information into Crashpad.";
    } else {
      SB_LOG(INFO) << "Loaded Cobalt library information into Crashpad.";
    }

    auto get_evergreen_sabi_string_func = reinterpret_cast<const char* (*)()>(
        library_loader->Resolve("GetEvergreenSabiString"));

    if (!CheckSabi(get_evergreen_sabi_string_func)) {
      SB_LOG(ERROR) << "CheckSabi failed";
      // Hard failure. Discard the image and auto rollback, but only if
      // the current image is not the system image.
      if (current_installation != 0) {
        current_installation =
            RevertBack(current_installation, app_key, true /* mark_bad */,
                       SlotSelectionStatus::kRollBackFailedToCheckSabi);
        continue;
      } else {
        // The system image at index 0 failed.
        return NULL;
      }
    }

    auto get_user_agent_func = reinterpret_cast<const char* (*)()>(
        library_loader->Resolve("GetCobaltUserAgentString"));
    if (!get_user_agent_func) {
      SB_LOG(ERROR) << "Failed to get user agent string";
    } else {
      std::vector<char> buffer(USER_AGENT_STRING_MAX_SIZE);
      starboard::strlcpy(buffer.data(), get_user_agent_func(),
                         USER_AGENT_STRING_MAX_SIZE);
      if (crashpad::InsertCrashpadAnnotation(
              crashpad::kCrashpadUserAgentStringKey, buffer.data())) {
        SB_DLOG(INFO) << "Added user agent string to Crashpad.";
      } else {
        SB_DLOG(INFO) << "Failed to add user agent string to Crashpad.";
      }
    }

    SB_DLOG(INFO) << "Successfully loaded Cobalt!\n";
    void* p = library_loader->Resolve("SbEventHandle");
    if (p != NULL) {
      SB_DLOG(INFO) << "Symbol Lookup succeeded address: " << p;
      return p;
    } else {
      SB_LOG(ERROR) << "Symbol Lookup failed\n";

      // Hard failure. Discard the image and auto rollback, but only if
      // the current image is not the system image.
      if (current_installation != 0) {
        current_installation =
            RevertBack(current_installation, app_key, true /* mark_bad */,
                       SlotSelectionStatus::kRollBackFailedToLookUpSymbols);
        continue;
      } else {
        // The system image at index 0 failed.
        return NULL;
      }
    }
  }
  return NULL;
}

}  // namespace loader_app

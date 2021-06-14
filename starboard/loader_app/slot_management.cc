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

#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"
#include "starboard/elf_loader/elf_loader_constants.h"
#include "starboard/elf_loader/sabi_string.h"
#include "starboard/event.h"
#include "starboard/file.h"
#include "starboard/loader_app/app_key_files.h"
#include "starboard/loader_app/drain_file.h"
#include "starboard/loader_app/installation_manager.h"
#include "starboard/memory.h"
#include "starboard/string.h"
#include "third_party/crashpad/wrapper/wrapper.h"

namespace starboard {
namespace loader_app {
namespace {

// The max number of installations slots.
const int kMaxNumInstallations = 3;

// Relative path for the Cobalt so file.
const char kCobaltLibraryPath[] = "lib";

// Filename for the Cobalt binary.
const char kCobaltLibraryName[] = "libcobalt.so";

// Relative path for the content directory of
// the Cobalt installation.
const char kCobaltContentPath[] = "content";

}  // namespace

int RevertBack(int current_installation, const std::string& app_key) {
  SB_LOG(INFO) << "RevertBack current_installation=" << current_installation;
  SB_DCHECK(current_installation != 0);
  std::vector<char> installation_path(kSbFileMaxPath);
  if (ImGetInstallationPath(current_installation, installation_path.data(),
                            kSbFileMaxPath) != IM_ERROR) {
    std::string bad_app_key_file_path =
        starboard::loader_app::GetBadAppKeyFilePath(installation_path.data(),
                                                    app_key);
    if (bad_app_key_file_path.empty()) {
      SB_LOG(WARNING) << "Failed to get bad app key file path for path="
                      << installation_path.data() << " and app_key=" << app_key;
    } else {
      if (!starboard::loader_app::CreateAppKeyFile(bad_app_key_file_path)) {
        SB_LOG(WARNING) << "Failed to create bad app key file: "
                        << bad_app_key_file_path;
      }
    }
  } else {
    SB_LOG(WARNING) << "Failed to get installation path for index: "
                    << current_installation;
  }
  current_installation = ImRevertToSuccessfulInstallation();
  return current_installation;
}

bool CheckBadFileExists(const char* installation_path, const char* app_key) {
  std::string bad_app_key_file_path =
      starboard::loader_app::GetBadAppKeyFilePath(installation_path, app_key);
  SB_DCHECK(!bad_app_key_file_path.empty());
  SB_LOG(INFO) << "bad_app_key_file_path: " << bad_app_key_file_path;
  SB_LOG(INFO) << "bad_app_key_file_path SbFileExists: "
               << SbFileExists(bad_app_key_file_path.c_str());
  return !bad_app_key_file_path.empty() &&
         SbFileExists(bad_app_key_file_path.c_str());
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
      starboard::loader_app::GetGoodAppKeyFilePath(installation_path, app_key);
  if (good_app_key_file_path.empty()) {
    SB_LOG(WARNING) << "Failed to get good app key file path for app_key="
                    << app_key;
    return false;
  }

  if (!SbFileExists(good_app_key_file_path.c_str())) {
    if (!starboard::loader_app::CreateAppKeyFile(good_app_key_file_path)) {
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
                             LibraryLoader* library_loader) {
  // Initialize the Installation Manager.
  SB_CHECK(ImInitialize(kMaxNumInstallations, app_key.c_str()) == IM_SUCCESS)
      << "Abort. Failed to initialize Installation Manager";

  // Roll forward if needed.
  if (ImRollForwardIfNeeded() == IM_ERROR) {
    SB_LOG(WARNING) << "Failed to roll forward";
  }

  // TODO: Try to simplify the loop.
  // Loop by priority.
  int current_installation = ImGetCurrentInstallationIndex();
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
          current_installation = RevertBack(current_installation, app_key);
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
        current_installation = RevertBack(current_installation, app_key);
        continue;
      } else {
        // The system image at index 0 failed.
        return NULL;
      }
    }

    SB_DLOG(INFO) << "installation_path=" << installation_path.data();

    if (current_installation != 0) {
      // Cleanup expired drain files
      DrainFileClear(installation_path.data(), app_key.c_str(), true);

      // Check for bad file.
      if (CheckBadFileExists(installation_path.data(), app_key.c_str())) {
        SB_LOG(INFO) << "Bad app key file";
        current_installation = RevertBack(current_installation, app_key);
        continue;
      }
      // If the current installation is in use by an updater roll back.
      if (DrainFileDraining(installation_path.data(), "")) {
        SB_LOG(INFO) << "Active slot draining";
        current_installation = RevertBack(current_installation, app_key);
        continue;
      }
      // Adopt installation performed from different app.
      if (!AdoptInstallation(current_installation, installation_path.data(),
                             app_key.c_str())) {
        SB_LOG(INFO) << "Unable to adopt installation";
        current_installation = RevertBack(current_installation, app_key);
        continue;
      }
    }

    // installation_n/lib/libcobalt.so
    std::vector<char> lib_path(kSbFileMaxPath);
    SbStringFormatF(lib_path.data(), kSbFileMaxPath, "%s%s%s%s%s",
                    installation_path.data(), kSbFileSepString,
                    kCobaltLibraryPath, kSbFileSepString, kCobaltLibraryName);
    if (!SbFileExists(lib_path.data())) {
      // Try the compressed path if the binary doesn't exits.
      starboard::strlcat(lib_path.data(),
                         starboard::elf_loader::kCompressionSuffix,
                         kSbFileMaxPath);
    }
    SB_LOG(INFO) << "lib_path=" << lib_path.data();

    std::string content;
    if (alternative_content_path.empty()) {
      // installation_n/content
      std::vector<char> content_path(kSbFileMaxPath);
      SbStringFormatF(content_path.data(), kSbFileMaxPath, "%s%s%s",
                      installation_path.data(), kSbFileSepString,
                      kCobaltContentPath);
      content = content_path.data();
    } else {
      content = alternative_content_path.c_str();
    }

    SB_LOG(INFO) << "content=" << content;

    if (!library_loader->Load(lib_path.data(), content.c_str())) {
      SB_LOG(WARNING) << "Failed to load Cobalt!";

      // Hard failure. Discard the image and auto rollback, but only if
      // the current image is not the system image.
      if (current_installation != 0) {
        current_installation = RevertBack(current_installation, app_key);
        continue;
      } else {
        // The system image at index 0 failed.
        return NULL;
      }
    }

    EvergreenInfo evergreen_info;
    GetEvergreenInfo(&evergreen_info);
    if (!third_party::crashpad::wrapper::AddEvergreenInfoToCrashpad(
            evergreen_info)) {
      SB_LOG(ERROR)
          << "Could not send Cobalt library information into Crashapd.";
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
        current_installation = RevertBack(current_installation, app_key);
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
      CrashpadAnnotations cobalt_version_info;
      memset(&cobalt_version_info, 0, sizeof(CrashpadAnnotations));
      starboard::strlcpy(cobalt_version_info.user_agent_string,
                         get_user_agent_func(), USER_AGENT_STRING_MAX_SIZE);
      third_party::crashpad::wrapper::AddAnnotationsToCrashpad(
          cobalt_version_info);
      SB_DLOG(INFO) << "Added user agent string to Crashpad.";
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
        current_installation = RevertBack(current_installation, app_key);
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
}  // namespace starboard

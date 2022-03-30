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

#include "components/update_client/cobalt_slot_management.h"

#include <algorithm>
#include <vector>

#include "base/files/file_util.h"
#include "base/values.h"
#include "cobalt/updater/utils.h"
#include "components/update_client/utils.h"
#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/loader_app/app_key_files.h"
#include "starboard/loader_app/drain_file.h"

namespace update_client {

namespace {
bool CheckBadFileExists(const char* installation_path, const char* app_key) {
  std::string bad_app_key_file_path =
      starboard::loader_app::GetBadAppKeyFilePath(installation_path, app_key);
  SB_DCHECK(!bad_app_key_file_path.empty());
  LOG(INFO) << "bad_app_key_file_path: " << bad_app_key_file_path;
  LOG(INFO) << "bad_app_key_file_path SbFileExists: "
            << SbFileExists(bad_app_key_file_path.c_str());
  return !bad_app_key_file_path.empty() &&
         SbFileExists(bad_app_key_file_path.c_str());
}

uint64_t ComputeSlotSize(
    const CobaltExtensionInstallationManagerApi* installation_api,
    int index) {
  if (!installation_api) {
    LOG(WARNING) << "ComputeSlotSize: "
                 << "Missing installation manager extension.";
    return 0;
  }
  std::vector<char> installation_path(kSbFileMaxPath);
  if (installation_api->GetInstallationPath(index, installation_path.data(),
                                            kSbFileMaxPath) == IM_EXT_ERROR) {
    LOG(WARNING) << "ComputeSlotSize: "
                 << "Failed to get installation path for slot " << index;
    return 0;
  }
  int64_t slot_size =
      base::ComputeDirectorySize(base::FilePath(installation_path.data()));
  LOG(INFO) << "ComputeSlotSize: slot_size=" << slot_size;
  if (slot_size <= 0) {
    LOG(WARNING) << "ComputeSlotSize: "
                 << "Failed to compute slot " << index << " size";
    return 0;
  }
  return slot_size;
}
}  // namespace

CobaltSlotManagement::CobaltSlotManagement() : installation_api_(nullptr) {}

bool CobaltSlotManagement::Init(
    const CobaltExtensionInstallationManagerApi* installation_api) {
  LOG(INFO) << "CobaltSlotManagement::Init";

  installation_api_ = installation_api;

  // Make sure the index is reset
  installation_index_ = IM_EXT_INVALID_INDEX;
  if (!installation_api_) {
    LOG(ERROR) << "Failed to get installation manager";
    return false;
  }

  char app_key[IM_EXT_MAX_APP_KEY_LENGTH];
  if (installation_api_->GetAppKey(app_key, IM_EXT_MAX_APP_KEY_LENGTH) ==
      IM_EXT_ERROR) {
    LOG(ERROR) << "Failed to get app key.";
    return false;
  }
  app_key_ = app_key;
  initialized_ = true;
  return true;
}

bool CobaltSlotManagement::SelectSlot(base::FilePath* dir) {
  SB_DCHECK(initialized_);
  if (!initialized_) {
    return false;
  }
  LOG(INFO) << "CobaltSlotManagement::SelectSlot";
  int max_slots = installation_api_->GetMaxNumberInstallations();
  if (max_slots == IM_EXT_ERROR) {
    LOG(ERROR) << "Failed to get max number of slots";
    return false;
  }

  // Default invalid version.
  base::Version slot_candidate_version;
  int slot_candidate = -1;
  base::FilePath slot_candidate_path;

  // Iterate over all writeable slots - index >= 1.
  for (int i = 1; i < max_slots; i++) {
    LOG(INFO) << "CobaltSlotManagement::SelectSlot iterating slot=" << i;
    std::vector<char> installation_path(kSbFileMaxPath);
    if (installation_api_->GetInstallationPath(i, installation_path.data(),
                                               installation_path.size()) ==
        IM_EXT_ERROR) {
      LOG(ERROR) << "CobaltSlotManagement::SelectSlot: Failed to get "
                    "installation path for slot="
                 << i;
      continue;
    }

    SB_DLOG(INFO) << "CobaltSlotManagement::SelectSlot: installation_path = "
                  << installation_path.data();

    base::FilePath installation_dir(
        std::string(installation_path.begin(), installation_path.end()));

    // Cleanup all expired files from all apps.
    DrainFileClearExpired(installation_dir.value().c_str());

    // Cleanup all drain files from the current app.
    DrainFileClearForApp(installation_dir.value().c_str(), app_key_.c_str());

    base::Version version =
        cobalt::updater::ReadEvergreenVersion(installation_dir);
    if (!version.IsValid()) {
      LOG(INFO) << "CobaltSlotManagement::SelectSlot installed version invalid";
      if (!DrainFileIsAnotherAppDraining(installation_dir.value().c_str(),
                                         app_key_.c_str())) {
        LOG(INFO) << "CobaltSlotManagement::SelectSlot not draining";
        // Found empty slot.
        slot_candidate = i;
        slot_candidate_path = installation_dir;
        break;
      } else {
        // There is active draining from another updater so bail out.
        LOG(ERROR) << "CobaltSlotManagement::SelectSlot bailing out";
        return false;
      }
    } else if ((!slot_candidate_version.IsValid() ||
                slot_candidate_version > version)) {
      if (!DrainFileIsAnotherAppDraining(installation_dir.value().c_str(),
                                         app_key_.c_str())) {
        // Found a slot with older version that's not draining.
        LOG(INFO) << "CobaltSlotManagement::SelectSlot slot candidate: " << i;
        slot_candidate_version = version;
        slot_candidate = i;
        slot_candidate_path = installation_dir;
      } else {
        // There is active draining from another updater so bail out.
        LOG(ERROR) << "CobaltSlotManagement::SelectSlot bailing out";
        return false;
      }
    }
  }

  installation_index_ = slot_candidate;
  *dir = slot_candidate_path;

  if (installation_index_ == -1 ||
      !DrainFileTryDrain(dir->value().c_str(), app_key_.c_str())) {
    LOG(ERROR)
        << "CobaltSlotManagement::SelectSlot unable to find a slot, candidate="
        << installation_index_;
    return false;
  }
  return true;
}

bool CobaltSlotManagement::ConfirmSlot(const base::FilePath& dir) {
  SB_DCHECK(initialized_);
  if (!initialized_) {
    return false;
  }
  LOG(INFO) << "CobaltSlotManagement::ConfirmSlot ";
  if (!DrainFileRankAndCheck(dir.value().c_str(), app_key_.c_str())) {
    LOG(INFO) << "CobaltSlotManagement::ConfirmSlot: failed to lock slot ";
    return false;
  }

  // TODO: Double check the installed_version.

  // Use the installation slot
  LOG(INFO) << "Resetting the slot: " << installation_index_;
  if (installation_api_->ResetInstallation(installation_index_) ==
      IM_EXT_ERROR) {
    LOG(INFO) << "CobaltSlotManagement::ConfirmSlot: failed to reset slot ";
    return false;
  }

  // Remove all files and directories except for our ranking drain file.
  DrainFilePrepareDirectory(dir.value().c_str(), app_key_.c_str());

  return true;
}

void CobaltSlotManagement::CleanupAllDrainFiles() {
  SB_DCHECK(initialized_);
  if (!initialized_) {
    return;
  }

  int max_slots = installation_api_->GetMaxNumberInstallations();
  if (max_slots == IM_EXT_ERROR) {
    LOG(ERROR) << "CobaltSlotManagement::CleanupAllDrainFile: Failed to get "
                  "max number of slots";
    return;
  }

  // Iterate over all writeable installation slots.
  for (int i = 1; i < max_slots; i++) {
    LOG(INFO) << "CobaltSlotManagement::CleanupAllDrainFile iterating slot="
              << i;
    std::vector<char> installation_path(kSbFileMaxPath);
    if (installation_api_->GetInstallationPath(i, installation_path.data(),
                                               installation_path.size()) ==
        IM_EXT_ERROR) {
      LOG(ERROR) << "CobaltSlotManagement::CleanupAllDrainFile: Failed to get "
                    "installation path for slot="
                 << i;
      continue;
    }

    LOG(INFO)
        << "CobaltSlotManagement::CleanupAllDrainFile: installation_path = "
        << installation_path.data();

    base::FilePath installation_dir(
        std::string(installation_path.begin(), installation_path.end()));

    DrainFileClearForApp(installation_dir.value().c_str(), app_key_.c_str());
  }
}

int CobaltSlotManagement::GetInstallationIndex() {
  SB_DCHECK(initialized_);
  if (!initialized_) {
    return false;
  }
  return installation_index_;
}

bool CobaltFinishInstallation(
    const CobaltExtensionInstallationManagerApi* installation_api,
    int installation_index,
    const std::string& dir,
    const std::string& app_key) {
  std::string good_app_key_file_path =
      starboard::loader_app::GetGoodAppKeyFilePath(dir, app_key);
  SB_CHECK(!good_app_key_file_path.empty());
  if (!starboard::loader_app::CreateAppKeyFile(good_app_key_file_path)) {
    LOG(WARNING) << "Failed to create good app key file";
  }
  DrainFileClearForApp(dir.c_str(), app_key.c_str());
  int ret =
      installation_api->RequestRollForwardToInstallation(installation_index);
  if (ret == IM_EXT_ERROR) {
    LOG(ERROR) << "Failed to request roll forward.";
    return false;
  }
  return true;
}

bool CobaltQuickUpdate(
    const CobaltExtensionInstallationManagerApi* installation_api,
    const base::Version& current_version) {
  if (!current_version.IsValid()) {
    return false;
  }
  char app_key[IM_EXT_MAX_APP_KEY_LENGTH];
  if (installation_api->GetAppKey(app_key, IM_EXT_MAX_APP_KEY_LENGTH) ==
      IM_EXT_ERROR) {
    LOG(ERROR) << "CobaltQuickUpdate: Failed to get app key.";
    return true;
  }

  int max_slots = installation_api->GetMaxNumberInstallations();
  if (max_slots == IM_EXT_ERROR) {
    LOG(ERROR) << "CobaltQuickUpdate: Failed to get max number of slots.";
    return true;
  }

  // We'll find the newest version of the installation that satisfies the
  // requirements as the final candidate slot.
  base::Version slot_candidate_version("1.0.1");
  int slot_candidate = -1;

  // Iterate over all writeable slots - index >= 1.
  for (int i = 1; i < max_slots; i++) {
    LOG(INFO) << "CobaltQuickInstallation: iterating slot=" << i;
    // Get the path to new installation.
    std::vector<char> installation_path(kSbFileMaxPath);
    if (installation_api->GetInstallationPath(i, installation_path.data(),
                                              installation_path.size()) ==
        IM_EXT_ERROR) {
      LOG(ERROR) << "CobaltQuickInstallation: Failed to get "
                 << "installation path for slot=" << i;
      continue;
    }

    SB_DLOG(INFO) << "CobaltQuickInstallation: installation_path = "
                  << installation_path.data();

    base::FilePath installation_dir = base::FilePath(
        std::string(installation_path.begin(), installation_path.end()));

    base::Version installed_version =
        cobalt::updater::ReadEvergreenVersion(installation_dir);

    std::string good_app_key_file_path =
        starboard::loader_app::GetGoodAppKeyFilePath(
            installation_dir.value().c_str(), app_key);
    if (!installed_version.IsValid()) {
      LOG(WARNING) << "CobaltQuickInstallation: invalid version ";
      continue;
    } else if (slot_candidate_version < installed_version &&
               current_version < installed_version &&
               !DrainFileIsAnotherAppDraining(installation_dir.value().c_str(),
                                              app_key) &&
               !CheckBadFileExists(installation_dir.value().c_str(), app_key) &&
               !SbFileExists(good_app_key_file_path.c_str()) &&
               starboard::loader_app::AnyGoodAppKeyFile(
                   installation_dir.value().c_str())) {
      // Found a slot with newer version than the current version that's not
      // draining, and no bad file of current app exists, and a good file
      // exists from a another app, but not from the current app.
      // The final candidate is the newest version of the valid
      // candidates.
      LOG(INFO) << "CobaltQuickInstallation: slot candidate: " << i;
      slot_candidate_version = installed_version;
      slot_candidate = i;
    }
  }

  if (slot_candidate != -1) {
    if (installation_api->RequestRollForwardToInstallation(slot_candidate) !=
        IM_EXT_ERROR) {
      LOG(INFO) << "CobaltQuickInstallation: quick update succeeded.";
      return true;
    }
  }
  LOG(WARNING) << "CobaltQuickInstallation: quick update failed.";
  return false;
}

bool CobaltSkipUpdate(
    const CobaltExtensionInstallationManagerApi* installation_api,
    uint64_t min_free_space_bytes,
    int64_t free_space_bytes,
    uint64_t installation_cleanup_size) {
  LOG(INFO) << "CobaltSkipUpdate: "
            << " min_free_space_bytes=" << min_free_space_bytes
            << " free_space_bytes=" << free_space_bytes
            << " installation_cleanup_size=" << installation_cleanup_size;

  if (free_space_bytes < 0) {
    LOG(WARNING) << "CobaltSkipUpdate: "
                 << "Unable to determine free space";
    return false;
  }

  if (free_space_bytes + installation_cleanup_size < min_free_space_bytes) {
    LOG(WARNING) << "CobaltSkipUpdate: Not enough free space";
    return true;
  }

  return false;
}

uint64_t CobaltInstallationCleanupSize(
    const CobaltExtensionInstallationManagerApi* installation_api) {
  if (!installation_api) {
    LOG(WARNING) << "CobaltInstallationCleanupSize: "
                 << "Missing installation manager extension.";
    return 0;
  }
  int max_slots = installation_api->GetMaxNumberInstallations();
  if (max_slots == IM_EXT_ERROR) {
    LOG(ERROR)
        << "CobaltInstallationCleanupSize: Failed to get max number of slots.";
    return 0;
  }
  // Ignore the system slot 0 and start with slot 1.
  uint64_t min_slot_size = ComputeSlotSize(installation_api, 1);
  for (int i = 2; i < max_slots; i++) {
    uint64_t slot_size = ComputeSlotSize(installation_api, i);
    if (slot_size < min_slot_size) {
      min_slot_size = slot_size;
    }
  }

  return min_slot_size;
}
}  // namespace update_client

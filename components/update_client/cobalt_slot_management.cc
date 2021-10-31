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
  SB_LOG(INFO) << "bad_app_key_file_path: " << bad_app_key_file_path;
  SB_LOG(INFO) << "bad_app_key_file_path SbFileExists: "
               << SbFileExists(bad_app_key_file_path.c_str());
  return !bad_app_key_file_path.empty() &&
         SbFileExists(bad_app_key_file_path.c_str());
}
}  // namespace

CobaltSlotManagement::CobaltSlotManagement() : installation_api_(nullptr) {}

bool CobaltSlotManagement::Init(
    const CobaltExtensionInstallationManagerApi* installation_api) {
  SB_LOG(INFO) << "CobaltSlotManagement::Init";

  installation_api_ = installation_api;

  // Make sure the index is reset
  installation_index_ = IM_EXT_INVALID_INDEX;
  if (!installation_api_) {
    SB_LOG(ERROR) << "Failed to get installation manager";
    return false;
  }

  char app_key[IM_EXT_MAX_APP_KEY_LENGTH];
  if (installation_api_->GetAppKey(app_key, IM_EXT_MAX_APP_KEY_LENGTH) ==
      IM_EXT_ERROR) {
    SB_LOG(ERROR) << "Failed to get app key.";
    return false;
  }
  app_key_ = app_key;
  return true;
}

bool CobaltSlotManagement::SelectSlot(base::FilePath* dir) {
  SB_DCHECK(installation_api_);
  SB_LOG(INFO) << "CobaltSlotManagement::SelectSlot";
  int max_slots = installation_api_->GetMaxNumberInstallations();
  if (max_slots == IM_EXT_ERROR) {
    SB_LOG(ERROR) << "Failed to get max number of slots";
    return false;
  }

  // Default invalid version.
  base::Version slot_candidate_version;
  int slot_candidate = -1;
  base::FilePath slot_candidate_path;

  // Iterate over all writeable slots - index >= 1.
  for (int i = 1; i < max_slots; i++) {
    SB_LOG(INFO) << "CobaltSlotManagement::SelectSlot iterating slot=" << i;
    std::vector<char> installation_path(kSbFileMaxPath);
    if (installation_api_->GetInstallationPath(i, installation_path.data(),
                                               installation_path.size()) ==
        IM_EXT_ERROR) {
      SB_LOG(ERROR) << "CobaltSlotManagement::SelectSlot: Failed to get "
                       "installation path for slot="
                    << i;
      continue;
    }

    SB_DLOG(INFO) << "CobaltSlotManagement::SelectSlot: installation_path = "
                  << installation_path.data();

    base::FilePath installation_dir(
        std::string(installation_path.begin(), installation_path.end()));

    // Cleanup expired drain files.
    DrainFileClear(installation_dir.value().c_str(), app_key_.c_str(), true);

    // Cleanup all drain files from the current app.
    DrainFileRemove(installation_dir.value().c_str(), app_key_.c_str());
    base::Version version =
        cobalt::updater::ReadEvergreenVersion(installation_dir);
    if (!version.IsValid()) {
      SB_LOG(INFO)
          << "CobaltSlotManagement::SelectSlot installed version invalid";
      if (!DrainFileDraining(installation_dir.value().c_str(), "")) {
        SB_LOG(INFO) << "CobaltSlotManagement::SelectSlot not draining";
        // Found empty slot.
        slot_candidate = i;
        slot_candidate_path = installation_dir;
        break;
      } else {
        // There is active draining from another updater so bail out.
        SB_LOG(ERROR) << "CobaltSlotManagement::SelectSlot bailing out";
        return false;
      }
    } else if ((!slot_candidate_version.IsValid() ||
                slot_candidate_version > version)) {
      if (!DrainFileDraining(installation_dir.value().c_str(), "")) {
        // Found a slot with older version that's not draining.
        SB_LOG(INFO) << "CobaltSlotManagement::SelectSlot slot candidate: "
                     << i;
        slot_candidate_version = version;
        slot_candidate = i;
        slot_candidate_path = installation_dir;
      } else {
        // There is active draining from another updater so bail out.
        SB_LOG(ERROR) << "CobaltSlotManagement::SelectSlot bailing out";
        return false;
      }
    }
  }

  installation_index_ = slot_candidate;
  *dir = slot_candidate_path;

  if (installation_index_ == -1 ||
      !DrainFileTryDrain(dir->value().c_str(), app_key_.c_str())) {
    SB_LOG(ERROR)
        << "CobaltSlotManagement::SelectSlot unable to find a slot, candidate="
        << installation_index_;
    return false;
  }
  return true;
}

bool CobaltSlotManagement::ConfirmSlot(const base::FilePath& dir) {
  SB_DCHECK(installation_api_);
  SB_LOG(INFO) << "CobaltSlotManagement::ConfirmSlot ";
  if (!DrainFileRankAndCheck(dir.value().c_str(), app_key_.c_str())) {
    SB_LOG(INFO) << "CobaltSlotManagement::ConfirmSlot: failed to lock slot ";
    return false;
  }

  // TODO: Double check the installed_version.

  // Use the installation slot
  SB_LOG(INFO) << "Resetting the slot: " << installation_index_;
  if (installation_api_->ResetInstallation(installation_index_) ==
      IM_EXT_ERROR) {
    SB_LOG(INFO) << "CobaltSlotManagement::ConfirmSlot: failed to reset slot ";
    return false;
  }

  // Remove all files and directories except for our ranking drain file.
  DrainFilePrepareDirectory(dir.value().c_str(), app_key_.c_str());

  return true;
}

void CobaltSlotManagement::CleanupAllDrainFiles(const base::FilePath& dir) {
  if (!dir.empty() && !app_key_.empty()) {
    DrainFileRemove(dir.value().c_str(), app_key_.c_str());
  }
}

int CobaltSlotManagement::GetInstallationIndex() {
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
    SB_LOG(WARNING) << "Failed to create good app key file";
  }
  DrainFileRemove(dir.c_str(), app_key.c_str());
  int ret =
      installation_api->RequestRollForwardToInstallation(installation_index);
  if (ret == IM_EXT_ERROR) {
    SB_LOG(ERROR) << "Failed to request roll forward.";
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
    SB_LOG(ERROR) << "CobaltQuickUpdate: Failed to get app key.";
    return true;
  }

  int max_slots = installation_api->GetMaxNumberInstallations();
  if (max_slots == IM_EXT_ERROR) {
    SB_LOG(ERROR) << "CobaltQuickUpdate: Failed to get max number of slots.";
    return true;
  }

  // We'll find the newest version of the installation that satisfies the
  // requirements as the final candidate slot.
  base::Version slot_candidate_version("1.0.1");
  int slot_candidate = -1;

  // Iterate over all writeable slots - index >= 1.
  for (int i = 1; i < max_slots; i++) {
    SB_LOG(INFO) << "CobaltQuickInstallation: iterating slot=" << i;
    // Get the path to new installation.
    std::vector<char> installation_path(kSbFileMaxPath);
    if (installation_api->GetInstallationPath(i, installation_path.data(),
                                              installation_path.size()) ==
        IM_EXT_ERROR) {
      SB_LOG(ERROR) << "CobaltQuickInstallation: Failed to get "
                    << "installation path for slot=" << i;
      continue;
    }

    SB_DLOG(INFO) << "CobaltQuickInstallation: installation_path = "
                  << installation_path.data();

    base::FilePath installation_dir = base::FilePath(
        std::string(installation_path.begin(), installation_path.end()));

    base::Version installed_version =
        cobalt::updater::ReadEvergreenVersion(installation_dir);

    if (!installed_version.IsValid()) {
      SB_LOG(WARNING) << "CobaltQuickInstallation: invalid version ";
      continue;
    } else if (slot_candidate_version < installed_version &&
               current_version < installed_version &&
               !DrainFileDraining(installation_dir.value().c_str(), "") &&
               !CheckBadFileExists(installation_dir.value().c_str(), app_key) &&
               starboard::loader_app::AnyGoodAppKeyFile(
                   installation_dir.value().c_str())) {
      // Found a slot with newer version than the current version that's not
      // draining, and no bad file of current app exists, and a good file
      // exists. The final candidate is the newest version of the valid
      // candidates.
      SB_LOG(INFO) << "CobaltQuickInstallation: slot candidate: " << i;
      slot_candidate_version = installed_version;
      slot_candidate = i;
    }
  }

  if (slot_candidate != -1) {
    if (installation_api->RequestRollForwardToInstallation(slot_candidate) !=
        IM_EXT_ERROR) {
      SB_LOG(INFO) << "CobaltQuickInstallation: quick update succeeded.";
      return true;
    }
  }
  SB_LOG(WARNING) << "CobaltQuickInstallation: quick update failed.";
  return false;
}

}  // namespace update_client

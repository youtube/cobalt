// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <tuple>
#include "base/files/file_util.h"

namespace update_client {

CobaltSlotManagement::CobaltSlotManagement() {
  std::ignore = installation_api_;
  std::ignore = app_key_;
  std::ignore = initialized_;
}

bool CobaltSlotManagement::Init(const CobaltExtensionInstallationManagerApi* installation_api) {
  installation_api_ = installation_api;
  initialized_ = true;
  installation_index_ = 0;
  return true;
}

bool CobaltSlotManagement::SelectSlot(base::FilePath* dir) {
  base::FilePath temp_dir;
  if (base::GetTempDir(&temp_dir)) {
    *dir = temp_dir.Append("cobalt_updater_test_slot");
    base::CreateDirectory(*dir);
    return true;
  }
  return false;
}

bool CobaltSlotManagement::ConfirmSlot(const base::FilePath& dir) {
  return true;
}

int CobaltSlotManagement::GetInstallationIndex() {
  return installation_index_;
}

void CobaltSlotManagement::CleanupAllDrainFiles() {}

bool CobaltFinishInstallation(
    const CobaltExtensionInstallationManagerApi* installation_api,
    int installation_index,
    const std::string& dir,
    const std::string& app_key) {
  if (!installation_api) {
    return false;
  }
  int ret = installation_api->RequestRollForwardToInstallation(installation_index);
  return ret != IM_EXT_ERROR;
}

bool CobaltQuickUpdate(
    const CobaltExtensionInstallationManagerApi* installation_api,
    const base::Version& current_version) {
  return false;
}

bool CobaltSkipUpdate(
    const CobaltExtensionInstallationManagerApi* installation_api,
    uint64_t min_free_space_bytes,
    int64_t free_space_bytes,
    uint64_t installation_cleanup_size) {
  return false;
}

uint64_t CobaltInstallationCleanupSize(
    const CobaltExtensionInstallationManagerApi* installation_api) {
  return 0;
}

}  // namespace update_client

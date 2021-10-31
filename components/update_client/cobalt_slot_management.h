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

#ifndef COMPONENTS_UPDATE_CLIENT_COBALT_SLOT_MANAGEMENT_H_
#define COMPONENTS_UPDATE_CLIENT_COBALT_SLOT_MANAGEMENT_H_

#include <string>

#include "base/files/file_path.h"
#include "base/version.h"
#include "cobalt/extension/installation_manager.h"

namespace update_client {

// Installation slot management logic for the Cobalt Updater.
// Multiple apps share the same installation slots on the file
// system but do keep individual metadata for the slot selection.
// Locking of the slots is performed through timestamped drain files.
// And each app marks the slot installation as good by using a
// good file.
class CobaltSlotManagement {
 public:
  CobaltSlotManagement();

  // Initialize with the |installation_api| extension.
  bool Init(const CobaltExtensionInstallationManagerApi* installation_api);

  // Selects the slot to use for the installation. The selected |dir| is
  // populated if the selection was successful.
  bool SelectSlot(base::FilePath* dir);

  // The slot selection will be confirmed if the function returns true.
  // The |dir| path will be used to perform the installation.
  bool ConfirmSlot(const base::FilePath& dir);

  CobaltSlotManagement(const CobaltSlotManagement&) = delete;
  CobaltSlotManagement& operator=(CobaltSlotManagement&&) = delete;
  void operator=(const CobaltSlotManagement&) = delete;

  // Returns the selected index for the installation slot.
  int GetInstallationIndex();

  // Cleanup all drain files of the current app.
  void CleanupAllDrainFiles(const base::FilePath& dir);

 private:
  int installation_index_ = IM_EXT_INVALID_INDEX;
  const CobaltExtensionInstallationManagerApi* installation_api_;
  std::string app_key_;
};

// Creates a good file and rolls forward to the installation in
// the specified |installation_index| and |dir| for the app
// identified by |app_key|.
bool CobaltFinishInstallation(
    const CobaltExtensionInstallationManagerApi* installation_api,
    int installation_index,
    const std::string& dir,
    const std::string& app_key);

// Performs a quick installation for the current app by
// upgrading it locally to the latest Cobalt version successfully
// installed by a different app.
bool CobaltQuickUpdate(
    const CobaltExtensionInstallationManagerApi* installation_api,
    const base::Version& current_version);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_COBALT_SLOT_MANAGEMENT_H_

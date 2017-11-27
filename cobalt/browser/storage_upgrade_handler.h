// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_STORAGE_UPGRADE_HANDLER_H_
#define COBALT_BROWSER_STORAGE_UPGRADE_HANDLER_H_

#include <string>

#include "cobalt/storage/storage_manager.h"

namespace cobalt {
namespace browser {

// Handles save data in upgrade format.
class StorageUpgradeHandler : public storage::StorageManager::UpgradeHandler {
 public:
  explicit StorageUpgradeHandler(const GURL& gurl);

  void OnUpgrade(storage::StorageManager* storage, const char* data,
                 int size) override;

  const std::string& default_local_storage_id() const {
    return default_local_storage_id_;
  }

 private:
  std::string default_local_storage_id_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_STORAGE_UPGRADE_HANDLER_H_

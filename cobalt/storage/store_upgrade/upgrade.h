// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_STORAGE_STORE_UPGRADE_UPGRADE_H_
#define COBALT_STORAGE_STORE_UPGRADE_UPGRADE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace cobalt {
namespace storage {
namespace store_upgrade {

// Checks whether store upgrade is required.
bool IsUpgradeRequired(const std::vector<uint8>& buffer);

// Upgrades the store blob.
bool UpgradeStore(std::vector<uint8>* buffer);

}  // namespace store_upgrade
}  // namespace storage
}  // namespace cobalt

#endif  // COBALT_STORAGE_STORE_UPGRADE_UPGRADE_H_

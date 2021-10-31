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

#include <memory>

#include "base/path_service.h"
#include "cobalt/base/get_application_key.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/storage/savegame.h"
#include "cobalt/storage/store/memory_store.h"
#include "cobalt/storage/store_upgrade/upgrade.h"

namespace cobalt {
namespace storage {
namespace store_upgrade {

size_t kMaxSaveGameSizeBytes = 4 * 1024 * 1024;
const char kDefaultURL[] = "https://www.youtube.com/tv";

int UpgradeMain(int argc, char** argv) {
  std::vector<uint8> buffer;
  Savegame::Options options;

  base::Optional<std::string> partition_key;
  partition_key = base::GetApplicationKey(GURL(kDefaultURL));

  options.id = partition_key;
  options.fallback_to_default_id = true;

  std::unique_ptr<Savegame> savegame = Savegame::Create(options);
  if (!savegame->Read(&buffer, kMaxSaveGameSizeBytes)) {
    DLOG(ERROR) << "Failed to read storage";
    return -1;
  }
  DLOG(INFO) << "read: size=" << buffer.size();
  if (IsUpgradeRequired(buffer)) {
    DLOG(INFO) << "Upgrade required";
    if (!UpgradeStore(&buffer)) {
      DLOG(ERROR) << "Failed to upgrade";
      return -1;
    }
  }

  if (!savegame->Write(buffer)) {
    DLOG(ERROR) << "Failed to save";
    return -1;
  }
  DLOG(INFO) << "SUCCESS: upgraded size=" << buffer.size();
  return 0;
}

}  // namespace store_upgrade
}  // namespace storage
}  // namespace cobalt

COBALT_WRAP_SIMPLE_MAIN(cobalt::storage::store_upgrade::UpgradeMain);

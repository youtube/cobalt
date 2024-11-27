// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/reset_evergreen_update.h"

#include <cstring>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"

namespace starboard {
namespace loader_app {

bool ResetEvergreenUpdate() {
  std::vector<char> storage_dir(kSbFileMaxPath);
  if (!SbSystemGetPath(kSbSystemPathStorageDirectory, storage_dir.data(),
                       kSbFileMaxPath)) {
    SB_LOG(ERROR)
        << "ResetEvergreenUpdate: Failed to get kSbSystemPathStorageDirectory";
    return false;
  }

  return SbFileDeleteRecursive(storage_dir.data(), true);
}

}  // namespace loader_app
}  // namespace starboard

// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/posix/free_space.h"

#include <sys/statvfs.h>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/extension/free_space.h"

namespace starboard {
namespace shared {
namespace posix {

namespace {

int64_t MeasureFreeSpace(SbSystemPathId system_path_id) {
  std::vector<char> path(kSbFileMaxPath + 1);
  if (!SbSystemGetPath(system_path_id, path.data(), path.size())) {
    return -1;
  }
  struct statvfs s;
  if (statvfs(path.data(), &s) != 0) {
    return -1;
  }

  return s.f_bsize * s.f_bfree;
}

const CobaltExtensionFreeSpaceApi kFreeSpaceApi = {
    kCobaltExtensionFreeSpaceName,
    1,
    &MeasureFreeSpace,
};

}  // namespace

const void* GetFreeSpaceApi() {
  return &kFreeSpaceApi;
}

}  // namespace posix
}  // namespace shared
}  // namespace starboard

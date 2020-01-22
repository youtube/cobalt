// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/storage.h"

#include <vector>

#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/shared/starboard/file_storage/storage_internal.h"
#include "starboard/user.h"

bool SbStorageDeleteRecord(SbUser user, const char* name) {
  if (!SbUserIsValid(user)) {
    return false;
  }

  std::vector<char> path(kSbFileMaxPath);
  bool success = starboard::shared::starboard::GetUserStorageFilePath(
      user, name, path.data(), static_cast<int>(path.size()));
  if (!success) {
    return false;
  }

  return SbFileDelete(path.data());
}

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

// clang-format off
#include "starboard/common/storage.h"
// clang-format on

#include <fcntl.h>

#include <vector>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/shared/starboard/file_storage/storage_internal.h"

SbStorageRecord SbStorageOpenRecord(const char* name) {
  std::vector<char> path(kSbFileMaxPath);
  bool success = starboard::GetStorageFilePath(name, path.data(),
                                               static_cast<int>(path.size()));
  if (!success) {
    return kSbStorageInvalidRecord;
  }

  // This will always create the storage file, even if it is just opened and
  // closed without doing any operation.
  int file = open(path.data(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
  if (!starboard::IsValid(file)) {
    return kSbStorageInvalidRecord;
  }

  SbStorageRecord result = new SbStorageRecordPrivate();
  SB_DCHECK(SbStorageIsValidRecord(result));
  result->file = file;
  if (name) {
    result->name = name;
  }
  return result;
}

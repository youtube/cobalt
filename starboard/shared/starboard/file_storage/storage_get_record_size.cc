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

#include "starboard/common/file.h"
#include "starboard/shared/starboard/file_storage/storage_internal.h"

int64_t SbStorageGetRecordSize(SbStorageRecord record) {
  if (!SbStorageIsValidRecord(record)) {
    return -1;
  }

  if (!starboard::IsValid(record->file)) {
    return -1;
  }

  struct stat info;
  bool success = !fstat(record->file, &info);
  if (!success) {
    return -1;
  }

  return info.st_size;
}

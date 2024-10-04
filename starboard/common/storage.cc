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

namespace starboard {

StorageRecord::StorageRecord()
    :
#if SB_API_VERSION < 16
      user_(SbUserGetCurrent()),
#endif  // SB_API_VERSION < 16
      record_(kSbStorageInvalidRecord) {
  Initialize();
}

StorageRecord::StorageRecord(const char* name)
    :
#if SB_API_VERSION < 16
      user_(SbUserGetCurrent()),
#endif  // SB_API_VERSION < 16
      name_(name),
      record_(kSbStorageInvalidRecord) {
  Initialize();
}

#if SB_API_VERSION < 16
StorageRecord::StorageRecord(SbUser user)
    : user_(user), record_(kSbStorageInvalidRecord) {
  Initialize();
}

StorageRecord::StorageRecord(SbUser user, const char* name)
    : user_(user), name_(name), record_(kSbStorageInvalidRecord) {
  Initialize();
}
#endif  // SB_API_VERSION < 16

StorageRecord::~StorageRecord() {
  Close();
}

bool StorageRecord::IsValid() {
  return SbStorageIsValidRecord(record_);
}

int64_t StorageRecord::GetSize() {
  return SbStorageGetRecordSize(record_);
}

int64_t StorageRecord::Read(char* out_data, int64_t data_size) {
  return SbStorageReadRecord(record_, out_data, data_size);
}

bool StorageRecord::Write(const char* data, int64_t data_size) {
  return SbStorageWriteRecord(record_, data, data_size);
}

bool StorageRecord::Close() {
  if (SbStorageIsValidRecord(record_)) {
    SbStorageRecord record = record_;
    record_ = kSbStorageInvalidRecord;
    return SbStorageCloseRecord(record);
  }
  return false;
}

bool StorageRecord::Delete() {
  Close();
#if SB_API_VERSION < 16
  return SbStorageDeleteRecord(user_, name_.empty() ? NULL : name_.c_str());
#else
  return SbStorageDeleteRecord(name_.empty() ? NULL : name_.c_str());
#endif  // SB_API_VERSION < 16
}

void StorageRecord::Initialize() {
#if SB_API_VERSION < 16
  if (SbUserIsValid(user_)) {
    record_ = SbStorageOpenRecord(user_, name_.empty() ? NULL : name_.c_str());
  }
#else
  record_ = SbStorageOpenRecord(name_.empty() ? NULL : name_.c_str());
#endif  // SB_API_VERSION < 16
}

}  // namespace starboard

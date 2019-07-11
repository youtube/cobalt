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
    : user_(SbUserGetCurrent()), record_(kSbStorageInvalidRecord) {
  Initialize();
}

StorageRecord::StorageRecord(SbUser user)
    : user_(user), record_(kSbStorageInvalidRecord) {
  Initialize();
}

StorageRecord::StorageRecord(const char* name)
    : user_(SbUserGetCurrent()), name_(name), record_(kSbStorageInvalidRecord) {
  Initialize();
}

StorageRecord::StorageRecord(SbUser user, const char* name)
    : user_(user), name_(name), record_(kSbStorageInvalidRecord) {
  Initialize();
}

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
  if (!name_.empty()) {
    return SbStorageDeleteRecord(user_, name_.c_str());
  } else {
    return SbStorageDeleteRecord(user_, NULL);
  }
}

void StorageRecord::Initialize() {
  if (SbUserIsValid(user_)) {
    if (!name_.empty()) {
      record_ = SbStorageOpenRecord(user_, name_.c_str());
    } else {
      record_ = SbStorageOpenRecord(user_, NULL);
    }
  }
}

}  // namespace starboard

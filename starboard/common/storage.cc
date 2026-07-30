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

StorageRecord::StorageRecord() : record_(kSbStorageInvalidRecord) {
  Initialize();
}

StorageRecord::StorageRecord(const char* name)
    : name_(name), record_(kSbStorageInvalidRecord) {
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
  return SbStorageDeleteRecord(name_.empty() ? NULL : name_.c_str());
}

void StorageRecord::Initialize() {
  record_ = SbStorageOpenRecord(name_.empty() ? NULL : name_.c_str());
}

}  // namespace starboard

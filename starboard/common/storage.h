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

// Module Overview: Starboard StorageRecord module
//
// Implements a convenience class that builds on top of the core Starboard
// storage functionality.

#ifndef STARBOARD_COMMON_STORAGE_H_
#define STARBOARD_COMMON_STORAGE_H_

#include <string>

#include "starboard/configuration.h"
#include "starboard/storage.h"
#include "starboard/types.h"
#include "starboard/user.h"

namespace starboard {

class StorageRecord {
 public:
  StorageRecord();
  explicit StorageRecord(SbUser user);

  explicit StorageRecord(const char* name);
  StorageRecord(SbUser user, const char* name);

  ~StorageRecord();
  bool IsValid();
  int64_t GetSize();
  int64_t Read(char* out_data, int64_t data_size);

  bool Write(const char* data, int64_t data_size);
  bool Close();
  bool Delete();

 private:
  void Initialize();

  SbUser user_;
  std::string name_;
  SbStorageRecord record_;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_STORAGE_H_

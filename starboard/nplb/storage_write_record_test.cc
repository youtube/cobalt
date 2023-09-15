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

// Write and read are mostly tested in SbStorageReadRecordTest.

#include "starboard/common/storage.h"
#include "starboard/nplb/storage_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbStorageWriteRecordTest, RainyDayInvalidRecord) {
  char data[kStorageSize] = {0};
  EXPECT_FALSE(
      SbStorageWriteRecord(kSbStorageInvalidRecord, data, kStorageSize));
}

TEST(SbStorageWriteRecordTest, RainyDayNullBuffer) {
  SbStorageRecord record = OpenStorageRecord();
  EXPECT_FALSE(SbStorageWriteRecord(record, NULL, kStorageSize));
  EXPECT_TRUE(SbStorageCloseRecord(record));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

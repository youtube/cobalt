// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/nplb/storage_helpers.h"
#include "starboard/storage.h"
#include "starboard/user.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbStorageReadRecordTest, SunnyDay) {
  ClearStorageRecord();

  SbStorageRecord record = OpenStorageRecord();
  WriteStorageRecord(record, kStorageSize);
  ReadAndCheckStorage(record, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2);
  EXPECT_TRUE(SbStorageCloseRecord(record));

  // Now we'll open again and make sure it is the same.
  record = OpenStorageRecord();
  ReadAndCheckStorage(record, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2);
  EXPECT_TRUE(SbStorageCloseRecord(record));
}

TEST(SbStorageReadRecordTest, SunnyDaySmallBuffer) {
  InitializeStorageRecord(kStorageSize);

  SbStorageRecord record = OpenStorageRecord();
  ReadAndCheckStorage(record, kStorageOffset, kStorageSize / 2,
                      kStorageSize / 2, kStorageSize);
  EXPECT_TRUE(SbStorageCloseRecord(record));
}

TEST(SbStorageReadRecordTest, SunnyDayNonexistant) {
  ClearStorageRecord();

  SbStorageRecord record = OpenStorageRecord();
  ReadAndCheckStorage(record, kStorageOffset, 0, 0, kStorageSize);
  EXPECT_TRUE(SbStorageCloseRecord(record));
}

TEST(SbStorageReadRecordTest, RainyDayInvalidRecord) {
  char data[kStorageSize] = {0};
  EXPECT_EQ(SB_INT64_C(-1),
            SbStorageReadRecord(kSbStorageInvalidRecord, data, kStorageSize));
}

TEST(SbStorageReadRecordTest, RainyDayNullBuffer) {
  SbStorageRecord record = OpenStorageRecord();
  EXPECT_TRUE(SbStorageIsValidRecord(record));
  EXPECT_EQ(SB_INT64_C(-1), SbStorageReadRecord(record, NULL, kStorageSize));
  EXPECT_TRUE(SbStorageCloseRecord(record));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

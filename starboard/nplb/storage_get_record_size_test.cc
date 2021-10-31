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
#include "starboard/nplb/storage_helpers.h"
#include "starboard/user.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbStorageGetRecordSizeTest, SunnyDay) {
  InitializeStorageRecord(kStorageSize);

  SbStorageRecord record = OpenStorageRecord();
  EXPECT_EQ(kStorageSize, SbStorageGetRecordSize(record));
  EXPECT_TRUE(SbStorageCloseRecord(record));
}

TEST(SbStorageGetRecordSizeTest, SunnyDayShrinkyDink) {
  InitializeStorageRecord(kStorageSize);

  SbStorageRecord record = OpenStorageRecord();
  EXPECT_EQ(kStorageSize, SbStorageGetRecordSize(record));
  // If we write over, it should truncate to the new size.
  WriteStorageRecord(record, kStorageSize / 2);
  WriteStorageRecord(record, kStorageSize / 4);
  EXPECT_TRUE(SbStorageCloseRecord(record));

  // Now we'll open again and make sure it is the same.
  record = OpenStorageRecord();
  EXPECT_EQ(kStorageSize / 4, SbStorageGetRecordSize(record));
  EXPECT_TRUE(SbStorageCloseRecord(record));
}

TEST(SbStorageGetRecordSizeTest, SunnyDayNonexistent) {
  ClearStorageRecord();
  SbStorageRecord record = OpenStorageRecord();
  EXPECT_EQ(SB_INT64_C(0), SbStorageGetRecordSize(record));
  EXPECT_TRUE(SbStorageCloseRecord(record));
}

TEST(SbStorageGetRecordSizeTest, RainyDayInvalidRecord) {
  EXPECT_EQ(SB_INT64_C(-1), SbStorageGetRecordSize(kSbStorageInvalidRecord));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

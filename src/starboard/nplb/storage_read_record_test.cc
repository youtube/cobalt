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
#include "starboard/nplb/file_helpers.h"
#include "starboard/nplb/storage_helpers.h"
#include "starboard/user.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbStorageReadRecordTest, SunnyDay) {
  int64_t pattern = 0;
  ClearStorageRecord();

  SbStorageRecord record = OpenStorageRecord();
  WriteStorageRecord(record, kStorageSize, pattern);
  ReadAndCheckStorage(record, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern);
  pattern = 6;
  // Write different data and check again.
  WriteStorageRecord(record, kStorageSize, pattern);
  ReadAndCheckStorage(record, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern);
  EXPECT_TRUE(SbStorageCloseRecord(record));

  // Now we'll open again and make sure it is the same.
  record = OpenStorageRecord();
  ReadAndCheckStorage(record, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern);
  EXPECT_TRUE(SbStorageCloseRecord(record));
  ClearStorageRecord();
}

TEST(SbStorageReadRecordTest, SunnyDaySmallBuffer) {
  InitializeStorageRecord(kStorageSize);

  SbStorageRecord record = OpenStorageRecord();
  ReadAndCheckStorage(record, kStorageOffset, kStorageSize / 2,
                      kStorageSize / 2, kStorageSize);
  EXPECT_TRUE(SbStorageCloseRecord(record));
}

TEST(SbStorageReadRecordTest, SunnyDayNamed) {
  int64_t pattern = 0;
  std::string name = ScopedRandomFile::MakeRandomFilename();
  ClearStorageRecord(name.c_str());

  SbStorageRecord record = OpenStorageRecord(name.c_str());
  WriteStorageRecord(record, kStorageSize, pattern);
  ReadAndCheckStorage(record, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern);
  // Write different data and check again.
  pattern = 6;
  WriteStorageRecord(record, kStorageSize, pattern);
  ReadAndCheckStorage(record, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern);
  EXPECT_TRUE(SbStorageCloseRecord(record));

  // Now we'll open again and make sure it is the same.
  record = OpenStorageRecord(name.c_str());
  ReadAndCheckStorage(record, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern);
  EXPECT_TRUE(SbStorageCloseRecord(record));

  ClearStorageRecord(name.c_str());
}

TEST(SbStorageReadRecordTest, SunnyDayNamed2) {
  int64_t pattern1 = 2;
  int64_t pattern2 = 8;
  std::string name1 = ScopedRandomFile::MakeRandomFilename();
  std::string name2 = ScopedRandomFile::MakeRandomFilename();
  ClearStorageRecord(name1.c_str());
  ClearStorageRecord(name2.c_str());

  SbStorageRecord record1 = OpenStorageRecord(name1.c_str());
  SbStorageRecord record2 = OpenStorageRecord(name2.c_str());
  WriteStorageRecord(record1, kStorageSize, pattern1);
  WriteStorageRecord(record2, kStorageSize, pattern2);
  ReadAndCheckStorage(record1, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern1);
  ReadAndCheckStorage(record2, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern2);
  EXPECT_TRUE(SbStorageCloseRecord(record1));
  EXPECT_TRUE(SbStorageCloseRecord(record2));

  // Now we'll open again and make sure they are the same.
  record1 = OpenStorageRecord(name1.c_str());
  record2 = OpenStorageRecord(name2.c_str());
  ReadAndCheckStorage(record1, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern1);
  ReadAndCheckStorage(record2, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern2);
  EXPECT_TRUE(SbStorageCloseRecord(record1));
  EXPECT_TRUE(SbStorageCloseRecord(record2));

  ClearStorageRecord(name1.c_str());
  ClearStorageRecord(name2.c_str());
}

TEST(SbStorageReadRecordTest, SunnyDayNamed3) {
  int64_t pattern1 = 3;
  int64_t pattern2 = 5;
  int64_t pattern3 = 7;
  std::string name1 = ScopedRandomFile::MakeRandomFilename();
  std::string name2 = ScopedRandomFile::MakeRandomFilename();
  ClearStorageRecord(name1.c_str());
  ClearStorageRecord(name2.c_str());
  ClearStorageRecord();

  SbStorageRecord record1 = OpenStorageRecord(name1.c_str());
  SbStorageRecord record2 = OpenStorageRecord(name2.c_str());
  SbStorageRecord record3 = OpenStorageRecord();
  WriteStorageRecord(record1, kStorageSize, pattern1);
  WriteStorageRecord(record2, kStorageSize, pattern2);
  WriteStorageRecord(record3, kStorageSize, pattern3);
  ReadAndCheckStorage(record1, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern1);
  ReadAndCheckStorage(record2, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern2);
  ReadAndCheckStorage(record3, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern3);
  EXPECT_TRUE(SbStorageCloseRecord(record1));
  EXPECT_TRUE(SbStorageCloseRecord(record2));
  EXPECT_TRUE(SbStorageCloseRecord(record3));

  // Now we'll open again and make sure they are the same.
  record1 = OpenStorageRecord(name1.c_str());
  record2 = OpenStorageRecord(name2.c_str());
  record3 = OpenStorageRecord();
  ReadAndCheckStorage(record1, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern1);
  ReadAndCheckStorage(record2, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern2);
  ReadAndCheckStorage(record3, kStorageOffset, kStorageSize, kStorageSize2,
                      kStorageSize2, pattern3);
  EXPECT_TRUE(SbStorageCloseRecord(record1));
  EXPECT_TRUE(SbStorageCloseRecord(record2));
  EXPECT_TRUE(SbStorageCloseRecord(record3));

  ClearStorageRecord(name1.c_str());
  ClearStorageRecord(name2.c_str());
  ClearStorageRecord();
}

TEST(SbStorageReadRecordTest, SunnyDayNonexistent) {
  ClearStorageRecord();

  SbStorageRecord record = OpenStorageRecord();
  ReadAndCheckStorage(record, kStorageOffset, 0, 0, kStorageSize);
  EXPECT_TRUE(SbStorageCloseRecord(record));
  ClearStorageRecord();
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

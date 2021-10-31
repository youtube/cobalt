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

#ifndef STARBOARD_NPLB_STORAGE_HELPERS_H_
#define STARBOARD_NPLB_STORAGE_HELPERS_H_

#include "starboard/common/storage.h"
#include "starboard/memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

const int64_t kStorageOffset = 100;
const int64_t kStorageSize = 1025;
const int64_t kStorageSize2 = kStorageSize * 2 + kStorageOffset;

// Deletes the storage for the current user.
static SB_C_INLINE void ClearStorageRecord() {
  SbStorageDeleteRecord(SbUserGetCurrent(), NULL);
}

// Deletes the named storage record for the current user.
static SB_C_INLINE void ClearStorageRecord(const char* name) {
  SbStorageDeleteRecord(SbUserGetCurrent(), name);
}

// Opens the storage record for the current user, validating that it is valid.
static SB_C_INLINE SbStorageRecord OpenStorageRecord() {
  SbStorageRecord record = SbStorageOpenRecord(SbUserGetCurrent(), NULL);
  EXPECT_TRUE(SbStorageIsValidRecord(record));
  return record;
}

// Opens the named storage record for the current user, validating that it is
// valid.
static SB_C_INLINE SbStorageRecord OpenStorageRecord(const char* name) {
  SbStorageRecord record = SbStorageOpenRecord(SbUserGetCurrent(), name);
  EXPECT_TRUE(SbStorageIsValidRecord(record));
  return record;
}

// Writes a standard pattern of |size| bytes into the given open storage
// |record|.
static SB_C_INLINE void WriteStorageRecord(SbStorageRecord record,
                                           int64_t size,
                                           int64_t pattern_offset = 0) {
  char* data = new char[size];
  for (int64_t i = 0; i < size; ++i) {
    data[i] = static_cast<char>((i + pattern_offset + 2) % 0xFF);
  }
  EXPECT_TRUE(SbStorageWriteRecord(record, data, size));
  EXPECT_EQ(size, SbStorageGetRecordSize(record));
  delete[] data;
}

// Ensures that the storage record for the current user is initialized with the
// standard pattern for exactly |length| bytes.

static SB_C_INLINE void InitializeStorageRecord(int64_t length) {
  ClearStorageRecord();
  SbStorageRecord record = OpenStorageRecord();
  WriteStorageRecord(record, length);
  EXPECT_TRUE(SbStorageCloseRecord(record));
}

// Checks a buffer of |total| size for the expected pattern (written in
// WriteStorageRecord) to start at |offset| and continue for |length|, and the
// rest of the buffer, before and after, should be set to 0.
static SB_C_INLINE void CheckStorageBuffer(char* data,
                                           int64_t offset,
                                           int64_t length,
                                           int64_t total,
                                           int64_t pattern_offset = 0) {
  for (int64_t i = 0; i < offset; ++i) {
    EXPECT_EQ(0, data[i]) << "i = " << i;
  }

  for (int64_t i = 0; i < length; ++i) {
    EXPECT_EQ(static_cast<char>((i + pattern_offset + 2) % 0xFF),
              data[i + offset])
        << "i=" << i;
  }

  for (int64_t i = length + offset; i < total; ++i) {
    EXPECT_EQ(0, data[i]) << "i = " << i;
  }
}

// Creates a temporary buffer of size |total|, reads |record| into the buffer at
// |offset| and reporting the buffer size as |length|, checks that the number of
// read bytes is |expected_length| and then checks the buffer for the expected
// pattern written in WriteStorageRecord over the expected range of the buffer.
static SB_C_INLINE void ReadAndCheckStorage(SbStorageRecord record,
                                            int64_t offset,
                                            int64_t expected_length,
                                            int64_t length,
                                            int64_t total,
                                            int64_t pattern_offset = 0) {
  char* data = new char[total];
  memset(data, 0, total);
  EXPECT_EQ(expected_length,
            SbStorageReadRecord(record, data + offset, length));
  CheckStorageBuffer(data, offset, expected_length, total, pattern_offset);
  delete[] data;
}

}  // namespace nplb
}  // namespace starboard

#endif  // STARBOARD_NPLB_STORAGE_HELPERS_H_

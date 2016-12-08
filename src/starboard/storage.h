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

// User-based Storage API. This is a simple all-at-once BLOB storage and
// retrieval API that is intended for robust long-term, per-user storage. Some
// platforms have different mechanisms for this kind of storage, so this API
// exists to allow a client application to access this kind of storage.

// Only a single open record can exist for each user. Attempting to open a
// second record for a user will result in undefined behavior.

// These APIs are NOT expected to be thread-safe, so either call them from a
// single thread, or perform proper synchronization around all calls.

#ifndef STARBOARD_STORAGE_H_
#define STARBOARD_STORAGE_H_

#include "starboard/export.h"
#include "starboard/types.h"
#include "starboard/user.h"

#ifdef __cplusplus
extern "C" {
#endif

// Private structure representing a single storage record.
typedef struct SbStorageRecordPrivate SbStorageRecordPrivate;

// A handle to an open storage record.
typedef SbStorageRecordPrivate* SbStorageRecord;

// Well-defined value for an invalid storage record handle.
#define kSbStorageInvalidRecord (SbStorageRecord) NULL

// Returns whether the given storage record handle is valid.
static SB_C_INLINE bool SbStorageIsValidRecord(SbStorageRecord record) {
  return record != kSbStorageInvalidRecord;
}

// Opens the storage record for |user|, providing a handle to the opened
// record. |user| must be a valid SbUser, or kSbStorageInvalidRecord will be
// returned. Performs blocking I/O on the calling thread.
SB_EXPORT SbStorageRecord SbStorageOpenRecord(SbUser user);

// Closes |record|, synchronously ensuring all written data is flushed, and
// returning whether the operation was successful. Storage writes should be as
// atomic as possible, so either the record should be fully written, or it
// should delete the record (or, even better, leave it untouched). |record| will
// be invalidated after this point, and subsequent calls to |record| will
// fail. Performs blocking I/O on the calling thread.
SB_EXPORT bool SbStorageCloseRecord(SbStorageRecord record);

// Returns the size of |record|, or -1 on error. Performs blocking I/O on the
// calling thread.
SB_EXPORT int64_t SbStorageGetRecordSize(SbStorageRecord record);

// Reads up to |data_size| bytes from |record| into |out_data|, starting at the
// beginning of the record. Returns the number of actual bytes read, which must
// be <= |size|, or -1 on error. This function makes a best-effort and performs
// blocking I/O on the calling thread.
SB_EXPORT int64_t SbStorageReadRecord(SbStorageRecord record,
                                      char* out_data,
                                      int64_t data_size);

// Replaces the data in |record| with |size| bytes from |data|. This always
// deletes any previous data in that record. Returns whether the write
// succeeded. This function makes a best-effort and performs blocking I/O on the
// calling thread.
SB_EXPORT bool SbStorageWriteRecord(SbStorageRecord record,
                                    const char* data,
                                    int64_t data_size);

// Deletes |user|'s storage record, returning whether the record both existed
// and was successfully deleted. This must not be called while |user|'s storage
// record is open. This function performs blocking I/O on the calling thread.
SB_EXPORT bool SbStorageDeleteRecord(SbUser user);

#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __cplusplus
namespace starboard {

// Inline scoped wrapper for SbStorageRecord.
class StorageRecord {
 public:
  StorageRecord()
      : user_(SbUserGetCurrent()), record_(kSbStorageInvalidRecord) {
    if (SbUserIsValid(user_)) {
      record_ = SbStorageOpenRecord(user_);
    }
  }

  explicit StorageRecord(SbUser user)
      : user_(user), record_(kSbStorageInvalidRecord) {
    if (SbUserIsValid(user_)) {
      record_ = SbStorageOpenRecord(user_);
    }
  }

  ~StorageRecord() { Close(); }
  bool IsValid() { return SbStorageIsValidRecord(record_); }
  int64_t GetSize() { return SbStorageGetRecordSize(record_); }
  int64_t Read(char* out_data, int64_t data_size) {
    return SbStorageReadRecord(record_, out_data, data_size);
  }

  bool Write(const char* data, int64_t data_size) {
    return SbStorageWriteRecord(record_, data, data_size);
  }

  bool Close() {
    if (SbStorageIsValidRecord(record_)) {
      SbStorageRecord record = record_;
      record_ = kSbStorageInvalidRecord;
      return SbStorageCloseRecord(record);
    }
    return false;
  }

  bool Delete() {
    Close();
    return SbStorageDeleteRecord(user_);
  }

 private:
  SbUser user_;
  SbStorageRecord record_;
};

}  // namespace starboard
#endif

#endif  // STARBOARD_STORAGE_H_

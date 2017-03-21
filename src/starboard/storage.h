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

// Module Overview: Starboard Storage module
//
// Defines a user-based Storage API. This is a simple, all-at-once BLOB storage
// and retrieval API that is intended for robust long-term, per-user storage.
// Some platforms have different mechanisms for this kind of storage, so this
// API exists to allow a client application to access this kind of storage.
//
// Note that there can be only one storage record per user and, thus, a maximum
// of one open storage record can exist for each user. Attempting to open a
// second record for a user will result in undefined behavior.
//
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

// Opens and returns a SbStorageRecord for |user|, blocking I/O on the calling
// thread until it returns. If |user| is not a valid |SbUser|, the function
// returns |kSbStorageInvalidRecord|.
//
// |user|: The user for which the storage record is opened.
SB_EXPORT SbStorageRecord SbStorageOpenRecord(SbUser user);

// Closes |record|, synchronously ensuring that all written data is flushed.
// This function performs blocking I/O on the calling thread.
//
// The return value indicates whether the operation succeeded. Storage writes
// should be as atomic as possible, so the record should either be fully
// written or deleted (or, even better, untouched).
//
// |record|: The storage record to close. |record| is invalid after this point,
// and subsequent calls referring to |record| will fail.
SB_EXPORT bool SbStorageCloseRecord(SbStorageRecord record);

// Returns the size of |record|, or |-1| if there is an error. This function
// performs blocking I/O on the calling thread.
//
// |record|: The record to retrieve the size of.
SB_EXPORT int64_t SbStorageGetRecordSize(SbStorageRecord record);

// Reads up to |data_size| bytes from |record|, starting at the beginning of
// the record. The function returns the actual number of bytes read, which
// must be <= |data_size|. The function returns |-1| in the event of an error.
// This function makes a best-effort to read the entire record, and it performs
// blocking I/O on the calling thread until the entire record is read or an
// error is encountered.
//
// |record|: The record to be read.
// |out_data|: The data read from the record.
// |data_size|: The amount of data, in bytes, to read.
SB_EXPORT int64_t SbStorageReadRecord(SbStorageRecord record,
                                      char* out_data,
                                      int64_t data_size);

// Replaces the data in |record| with |data_size| bytes from |data|. This
// function always deletes any previous data in that record. The return value
// indicates whether the write succeeded. This function makes a best-effort
// to read the entire record, and it performs performs blocking I/O on the
// calling thread until the entire record is read or an error is encountered.
//
// |record|: The record to be written to.
// |data|: The data to write to the record.
// |data_size|: The amount of |data|, in bytes, to write to the record. Thus,
//   if |data_size| is smaller than the total size of |data|, only part of
//   |data| is written to the record.
SB_EXPORT bool SbStorageWriteRecord(SbStorageRecord record,
                                    const char* data,
                                    int64_t data_size);

// Deletes the |SbStorageRecord| for the specified user. The return value
// indicates whether the record existed and was successfully deleted. If the
// record did not exist or could not be deleted, the function returns |false|.
//
// This function must not be called while the user's storage record is open.
// This function performs blocking I/O on the calling thread.
//
// |user|: The user for whom the record is deleted.
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

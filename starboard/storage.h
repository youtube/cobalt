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

// Module Overview: Starboard Storage module
//
// Defines the Storage API. This is a simple, all-at-once BLOB storage and
// retrieval API intended for robust, long-term storage. Because platforms use
// different storage mechanisms, this API provides a consistent interface for
// client applications.
//
// Only one storage record can exist; therefore, you can open at most one
// storage record at a time. Attempting to open a second record results in
// undefined behavior.
//
// These APIs are not thread-safe. Call them from a single thread, or use proper
// synchronization.

#ifndef STARBOARD_STORAGE_H_
#define STARBOARD_STORAGE_H_

#include <stddef.h>
#include <stdint.h>

#include "starboard/export.h"

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
static inline bool SbStorageIsValidRecord(SbStorageRecord record) {
  return record != kSbStorageInvalidRecord;
}

// Opens and returns the |SbStorageRecord| specified by |name|, blocking the
// calling thread until the operation completes. Returns an empty
// |SbStorageRecord| (size zero) if the record does not exist. Opening an
// already-open |SbStorageRecord| results in undefined behavior.
//
// If |name| is `NULL`, opens the default storage record (matching the behavior
// of previous API versions).
//
// * |name|: The filesystem-safe name of the record to open.
SB_EXPORT SbStorageRecord SbStorageOpenRecord(const char* name);

// Closes |record|, synchronously ensuring that all written data is flushed.
// This function performs blocking I/O on the calling thread.
//
// Returns whether the operation succeeded. Storage writes should be atomic; the
// record should either be fully written or remain unchanged.
//
// * |record|: The storage record to close. The handle becomes invalid, and
//   subsequent calls using it will fail.
SB_EXPORT bool SbStorageCloseRecord(SbStorageRecord record);

// Returns the size of |record|, or `-1` on error. This function performs
// blocking I/O on the calling thread.
//
// * |record|: The record to retrieve the size of.
SB_EXPORT int64_t SbStorageGetRecordSize(SbStorageRecord record);

// Reads up to |data_size| bytes from |record|, starting at the beginning of the
// record. Returns the actual number of bytes read (which is less than or equal
// to |data_size|), or `-1` on error. This function makes a best-effort to read
// the entire record, blocking the calling thread until the operation completes
// or fails.
//
// * |record|: The record to read.
// * |out_data|: The destination buffer for the read data.
// * |data_size|: The number of bytes to read.
SB_EXPORT int64_t SbStorageReadRecord(SbStorageRecord record,
                                      char* out_data,
                                      int64_t data_size);

// Replaces the data in |record| with |data_size| bytes from |data|, deleting
// any previous data. Returns whether the write succeeded. This function makes a
// best-effort to write the entire record, and may perform blocking I/O on the
// calling thread.
//
// Although `SbStorageWriteRecord()` may defer persistence, a subsequent
// `SbStorageReadRecord()` call must immediately reflect the write, even without
// calling `SbStorageCloseRecord()`. Data should persist shortly after writing,
// even in the event of unexpected process termination.
//
// * |record|: The record to write.
// * |data|: The data to write.
// * |data_size|: The number of bytes to write.
SB_EXPORT bool SbStorageWriteRecord(SbStorageRecord record,
                                    const char* data,
                                    int64_t data_size);

// Deletes the |SbStorageRecord| specified by |name|. Returns `true` if the
// record existed and was successfully deleted; otherwise, returns `false`.
//
// If |name| is `NULL`, this function deletes the default storage record
// (matching the behavior of previous API versions).
//
// Do not call this function while the storage record is open. This function
// performs blocking I/O on the calling thread.
//
// * |name|: The filesystem-safe name of the record to open.
SB_EXPORT bool SbStorageDeleteRecord(const char* name);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_STORAGE_H_

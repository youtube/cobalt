// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard File module
//
// Defines file system input/output functions.

#ifndef STARBOARD_FILE_H_
#define STARBOARD_FILE_H_

#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if SB_API_VERSION < 17

// Private structure representing an open file.
typedef struct SbFilePrivate SbFilePrivate;

// A handle to an open file.
typedef SbFilePrivate* SbFile;

// Flags that define how a file is used in the application. These flags should
// be or'd together when passed to SbFileOpen to open or create a file.
//
// The following five flags are mutually exclusive. You must specify exactly one
// of them:
// - |kSbFileOpenAlways|
// - |kSbFileOpenOnly|
// - |kSbFileOpenTruncated|
// - |kSbFileCreateAlways|
// - |kSbFileCreateOnly|
//
// In addition, one or more of the following flags must be specified:
// - |kSbFileRead|
// - |kSbFileWrite|
//
// The |kSbFileAsync| flag is optional.
typedef enum SbFileFlags {
  kSbFileOpenOnly = 1 << 0,       // Opens a file, only if it exists.
  kSbFileCreateOnly = 1 << 1,     // Creates a new file, only if it
                                  //   does not already exist.
  kSbFileOpenAlways = 1 << 2,     // Opens an existing file at the specified
                                  // path or creates a new file at that path.
  kSbFileCreateAlways = 1 << 3,   // Creates a new file at the specified path
                                  // or overwrites an existing file at that
                                  // path.
  kSbFileOpenTruncated = 1 << 4,  // Opens a file and truncates it to zero,
                                  // only if it exists.
  kSbFileRead = 1 << 5,
  kSbFileWrite = 1 << 6,
  kSbFileAsync = 1 << 7,  // May allow asynchronous I/O on some
                          //   platforms, meaning that calls to
                          //   Read or Write will only return the
                          //   data that is readily available.
} SbFileFlags;

// kSbFileErrorAccessDenied is returned when a call fails because of a
// filesystem restriction. kSbFileErrorSecurity is returned when a security
// policy doesn't allow the operation to be executed.
typedef enum SbFileError {
  kSbFileOk = 0,
  kSbFileErrorFailed = -1,
  kSbFileErrorInUse = -2,
  kSbFileErrorExists = -3,
  kSbFileErrorNotFound = -4,
  kSbFileErrorAccessDenied = -5,
  kSbFileErrorTooManyOpened = -6,
  kSbFileErrorNoMemory = -7,
  kSbFileErrorNoSpace = -8,
  kSbFileErrorNotADirectory = -9,
  kSbFileErrorInvalidOperation = -10,
  kSbFileErrorSecurity = -11,
  kSbFileErrorAbort = -12,
  kSbFileErrorNotAFile = -13,
  kSbFileErrorNotEmpty = -14,
  kSbFileErrorInvalidUrl = -15,
  kSbFileErrorIO = -16,
  kSbFileErrorMax = -17,
} SbFileError;

// This explicit mapping matches both FILE_ on Windows and SEEK_ on Linux.
typedef enum SbFileWhence {
  kSbFileFromBegin = 0,
  kSbFileFromCurrent = 1,
  kSbFileFromEnd = 2,
} SbFileWhence;

// Used to hold information about a file.
typedef struct SbFileInfo {
  // The size of the file in bytes. Undefined when is_directory is true.
  int64_t size;

  // Whether the file corresponds to a directory.
  bool is_directory;

  // Whether the file corresponds to a symbolic link.
  bool is_symbolic_link;

  // The last modified time of a file - microseconds since Windows epoch UTC.
  int64_t last_modified;

  // The last accessed time of a file - microseconds since Windows epoch UTC.
  int64_t last_accessed;

  // The creation time of a file - microseconds since Windows epoch UTC.
  int64_t creation_time;
} SbFileInfo;

// Well-defined value for an invalid file handle.
#define kSbFileInvalid (SbFile) NULL

// Returns whether the given file handle is valid.
static inline bool SbFileIsValid(SbFile file) {
  return file != kSbFileInvalid;
}

#endif

// Replaces the content of the file at |path| with |data|. Returns whether the
// contents of the file were replaced. The replacement of the content is an
// atomic operation. The file will either have all of the data, or none.
//
// |path|: The path to the file whose contents should be replaced.
// |data|: The data to replace the file contents with.
// |data_size|: The amount of |data|, in bytes, to be written to the file.
SB_EXPORT bool SbFileAtomicReplace(const char* path,
                                   const char* data,
                                   int64_t data_size);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_FILE_H_

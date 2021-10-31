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
#include "starboard/time.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

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

  // The last modified time of a file.
  SbTime last_modified;

  // The last accessed time of a file.
  SbTime last_accessed;

  // The creation time of a file.
  SbTime creation_time;
} SbFileInfo;

// Well-defined value for an invalid file handle.
#define kSbFileInvalid (SbFile) NULL

// Returns whether the given file handle is valid.
static SB_C_INLINE bool SbFileIsValid(SbFile file) {
  return file != kSbFileInvalid;
}

// Opens the file at |path|, which must be absolute, creating it if specified by
// |flags|. The read/write position is at the beginning of the file.
//
// Note that this function only guarantees the correct behavior when |path|
// points to a file. The behavior is undefined if |path| points to a directory.
//
// |path|: The absolute path of the file to be opened.
// |flags|: |SbFileFlags| that determine how the file is used in the
//   application. Among other things, |flags| can indicate whether the
//   application should create |path| if |path| does not already exist.
// |out_created|: Starboard sets this value to |true| if a new file is created
//   or if an old file is truncated to zero length to simulate a new file,
//   which can happen if the |kSbFileCreateAlways| flag is set. Otherwise,
//   Starboard sets this value to |false|.
// |out_error|: If |path| cannot be created, this is set to |kSbFileInvalid|.
//   Otherwise, it is |NULL|.
SB_EXPORT SbFile SbFileOpen(const char* path,
                            int flags,
                            bool* out_created,
                            SbFileError* out_error);

// Closes |file|. The return value indicates whether the file was closed
// successfully.
//
// |file|: The absolute path of the file to be closed.
SB_EXPORT bool SbFileClose(SbFile file);

#if SB_API_VERSION >= 12

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

#endif  // SB_API_VERSION >= 12

// Changes the current read/write position in |file|. The return value
// identifies the resultant current read/write position in the file (relative
// to the start) or |-1| in case of an error. This function might not support
// seeking past the end of the file on some platforms.
//
// |file|: The SbFile in which the read/write position will be changed.
// |whence|: The starting read/write position. The position is modified relative
//   to this value.
// |offset|: The amount that the read/write position is changed, relative to
//   |whence|.
SB_EXPORT int64_t SbFileSeek(SbFile file, SbFileWhence whence, int64_t offset);

// Reads |size| bytes (or until EOF is reached) from |file| into |data|,
// starting at the file's current position.
//
// The return value specifies the number of bytes read or |-1| if there was
// an error. Note that this function does NOT make a best effort to read all
// data on all platforms; rather, it just reads however many bytes are quickly
// available. However, this function can be run in a loop to make it a
// best-effort read.
//
// |file|: The SbFile from which to read data.
// |data|: The variable to which data is read.
// |size|: The amount of data (in bytes) to read.
SB_EXPORT int SbFileRead(SbFile file, char* data, int size);

// Writes the given buffer into |file| at the file's current position,
// overwriting any data that was previously there.
//
// The return value identifies the number of bytes written, or |-1| on error.
// Note that this function does NOT make a best effort to write all data;
// rather, it writes however many bytes can be written quickly. Generally, this
// means that it writes however many bytes as possible without blocking on IO.
// Run this function in a loop to ensure that all data is written.
//
// |file|: The SbFile to which data will be written.
// |data|: The data to be written.
// |size|: The amount of data (in bytes) to write.
SB_EXPORT int SbFileWrite(SbFile file, const char* data, int size);

// Truncates the given |file| to the given |length|. The return value indicates
// whether the file was truncated successfully.
//
// |file|: The file to be truncated.
// |length|: The expected length of the file after it is truncated. If |length|
//   is greater than the current size of the file, then the file is extended
//   with zeros. If |length| is negative, then the function is a no-op and
//   returns |false|.
SB_EXPORT bool SbFileTruncate(SbFile file, int64_t length);

// Flushes the write buffer to |file|. Data written via SbFileWrite is not
// necessarily committed until the SbFile is flushed or closed.
//
// |file|: The SbFile to which the write buffer is flushed.
SB_EXPORT bool SbFileFlush(SbFile file);

// Retrieves information about |file|. The return value indicates whether the
// file information was retrieved successfully.
//
// |file|: The SbFile for which information is retrieved.
// |out_info|: The variable into which the retrieved data is placed. This
//   variable is not touched if the operation is not successful.
SB_EXPORT bool SbFileGetInfo(SbFile file, SbFileInfo* out_info);

// Retrieves information about the file at |path|. The return value indicates
// whether the file information was retrieved successfully.
//
// |file|: The absolute path of the file for which information is retrieved.
// |out_info|: The variable into which the retrieved data is placed. This
//   variable is not touched if the operation is not successful.
SB_EXPORT bool SbFileGetPathInfo(const char* path, SbFileInfo* out_info);

// Deletes the regular file, symlink, or empty directory at |path|. This
// function is used primarily to clean up after unit tests. On some platforms,
// this function fails if the file in question is being held open.
//
// |path|: The absolute path of the file, symlink, or directory to be deleted.
SB_EXPORT bool SbFileDelete(const char* path);

// Indicates whether a file or directory exists at |path|.
//
// |path|: The absolute path of the file or directory being checked.
SB_EXPORT bool SbFileExists(const char* path);

// Indicates whether SbFileOpen() with the given |flags| is allowed for |path|.
//
// |path|: The absolute path to be checked.
// |flags|: The flags that are being evaluated for the given |path|.
SB_EXPORT bool SbFileCanOpen(const char* path, int flags);

// Converts an ISO |fopen()| mode string into flags that can be equivalently
// passed into SbFileOpen().
//
// |mode|: The mode string to be converted into flags.
SB_EXPORT int SbFileModeStringToFlags(const char* mode);

// Reads |size| bytes (or until EOF is reached) from |file| into |data|,
// starting at the file's current position.
//
// The return value specifies the number of bytes read or |-1| if there was
// an error. Note that, unlike |SbFileRead|, this function does make a best
// effort to read all data on all platforms. As such, it is not intended for
// stream-oriented files but instead for cases when the normal expectation is
// that |size| bytes can be read unless there is an error.
//
// |file|: The SbFile from which to read data.
// |data|: The variable to which data is read.
// |size|: The amount of data (in bytes) to read.
static inline int SbFileReadAll(SbFile file, char* data, int size) {
  if (!SbFileIsValid(file) || size < 0) {
    return -1;
  }
  int bytes_read = 0;
  int rv;
  do {
    rv = SbFileRead(file, data + bytes_read, size - bytes_read);
    if (rv <= 0) {
      break;
    }
    bytes_read += rv;
  } while (bytes_read < size);

  return bytes_read ? bytes_read : rv;
}

// Writes the given buffer into |file|, starting at the beginning of the file,
// and overwriting any data that was previously there. Unlike SbFileWrite, this
// function does make a best effort to write all data on all platforms.
//
// The return value identifies the number of bytes written, or |-1| on error.
//
// |file|: The file to which data will be written.
// |data|: The data to be written.
// |size|: The amount of data (in bytes) to write.
static inline int SbFileWriteAll(SbFile file, const char* data, int size) {
  if (!SbFileIsValid(file) || size < 0) {
    return -1;
  }
  int bytes_written = 0;
  int rv;
  do {
    rv = SbFileWrite(file, data + bytes_written, size - bytes_written);
    if (rv <= 0) {
      break;
    }
    bytes_written += rv;
  } while (bytes_written < size);

  return bytes_written ? bytes_written : rv;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#if SB_API_VERSION < 13 && defined(__cplusplus)
extern "C++" {
#include "starboard/common/file.h"
}  // extern "C++"
#endif  // SB_API_VERSION < 13 && defined(__cplusplus)

#endif  // STARBOARD_FILE_H_

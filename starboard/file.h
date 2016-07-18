// Copyright 2015 Google Inc. All Rights Reserved.
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

// File system Input/Output

#ifndef STARBOARD_FILE_H_
#define STARBOARD_FILE_H_

#include "starboard/export.h"
#include "starboard/log.h"
#include "starboard/time.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Private structure representing an open file.
typedef struct SbFilePrivate SbFilePrivate;

// A handle to an open file.
typedef SbFilePrivate* SbFile;

// kSbFile(Open|Create)(Only|Always|Truncated) are mutually exclusive. You
// should specify exactly one of the five (possibly combining with other flags)
// when opening or creating a file.
typedef enum SbFileFlags {
  kSbFileOpenOnly = 1 << 0,       // Opens a file, only if it exists.
  kSbFileCreateOnly = 1 << 1,     // Creates a new file, only if it
                                  //   does not already exist.
  kSbFileOpenAlways = 1 << 2,     // May create a new file.
  kSbFileCreateAlways = 1 << 3,   // May overwrite an old file.
  kSbFileOpenTruncated = 1 << 4,  // Opens a file and truncates it
                                  //   to zero, only if it exists.
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
  // Put new entries here and increment kSbFileErrorMax.
  kSbFileErrorMax = -16,
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
// |flags|. |out_created|, if provided, will be set to true if a new file was
// created (or an old one truncated to zero length to simulate a new file, which
// can happen with kSbFileCreateAlways), and false otherwise.  |out_error| can
// be NULL. If creation failed, it will return kSbFileInvalid. The read/write
// position is at the beginning of the file.
SB_EXPORT SbFile SbFileOpen(const char* path,
                            int flags,
                            bool* out_created,
                            SbFileError* out_error);

// Closes |file|. Returns whether the close was successful.
SB_EXPORT bool SbFileClose(SbFile file);

// Changes current read/write position in |file| to |offset| relative to the
// origin defined by |whence|. Returns the resultant current read/write position
// in the file (relative to the start) or -1 in case of error. May not support
// seeking past the end of the file on some platforms.
SB_EXPORT int64_t SbFileSeek(SbFile file, SbFileWhence whence, int64_t offset);

// Reads |size| bytes (or until EOF is reached) from |file| into |data|,
// starting at the file's current position. Returns the number of bytes read, or
// -1 on error. Note that this function does NOT make a best effort to read all
// data on all platforms, it just reads however many bytes are quickly
// available. Can be run in a loop to make it a best-effort read.
SB_EXPORT int SbFileRead(SbFile file, char* data, int size);

// Writes the given buffer into |file| at the file's current position,
// overwritting any data that was previously there. Returns the number of bytes
// written, or -1 on error. Note that this function does NOT make a best effort
// to write all data, it writes however many bytes can be written quickly. It
// should be run in a loop to ensure all data is written.
SB_EXPORT int SbFileWrite(SbFile file, const char* data, int size);

// Truncates the given |file| to the given |length|. If length is greater than
// the current size of the file, the file is extended with zeros. Negative
// |length| will do nothing and return false.
SB_EXPORT bool SbFileTruncate(SbFile file, int64_t length);

// Flushes the write buffer to |file|. Data written via SbFileWrite isn't
// necessarily comitted right away until the file is flushed or closed.
SB_EXPORT bool SbFileFlush(SbFile file);

// Places some information about |file|, an open SbFile, in |out_info|. Returns
// |true| if successful. If unsuccessful, |out_info| is untouched.
SB_EXPORT bool SbFileGetInfo(SbFile file, SbFileInfo* out_info);

// Places information about the file or directory at |path|, which must be
// absolute, into |out_info|. Returns |true| if successful. If unsuccessful,
// |out_info| is untouched.
SB_EXPORT bool SbFileGetPathInfo(const char* path, SbFileInfo* out_info);

// Deletes the regular file, symlink, or empty directory at |path|, which must
// be absolute. Used mainly to clean up after unit tests. May fail on some
// platforms if the file in question is being held open.
SB_EXPORT bool SbFileDelete(const char* path);

// Returns whether a file or directory exists at |path|, which must be absolute.
SB_EXPORT bool SbFileExists(const char* path);

// Returns whether SbFileOpen() with the given |flags| is allowed for |path|,
// which must be absolute.
SB_EXPORT bool SbFileCanOpen(const char* path, int flags);

// Converts an ISO fopen() mode string into flags that can be equivalently
// passed into SbFileOpen().
SB_EXPORT int SbFileModeStringToFlags(const char* mode);
#ifdef __cplusplus
}  // extern "C"
#endif

#ifdef __cplusplus
namespace starboard {

// A class that opens an SbFile in its constructor and closes it in its
// destructor, so the file is open for the lifetime of the object. Member
// functions call the corresponding SbFile function.
class ScopedFile {
 public:
  ScopedFile(const char* path,
             int flags,
             bool* out_created,
             SbFileError* out_error)
      : file_(kSbFileInvalid) {
    file_ = SbFileOpen(path, flags, out_created, out_error);
  }

  ScopedFile(const char* path, int flags, bool* out_created)
      : file_(kSbFileInvalid) {
    file_ = SbFileOpen(path, flags, out_created, NULL);
  }

  ScopedFile(const char* path, int flags) : file_(kSbFileInvalid) {
    file_ = SbFileOpen(path, flags, NULL, NULL);
  }

  ~ScopedFile() { SbFileClose(file_); }

  SbFile file() const { return file_; }

  bool IsValid() const { return SbFileIsValid(file_); }

  int64_t Seek(SbFileWhence whence, int64_t offset) const {
    return SbFileSeek(file_, whence, offset);
  }

  int Read(char* data, int size) const { return SbFileRead(file_, data, size); }

  int Write(const char* data, int size) const {
    return SbFileWrite(file_, data, size);
  }

  bool Truncate(int64_t length) const { return SbFileTruncate(file_, length); }

  bool Flush() const { return SbFileFlush(file_); }

  bool GetInfo(SbFileInfo* out_info) const {
    return SbFileGetInfo(file_, out_info);
  }

 private:
  SbFile file_;
};

}  // namespace starboard
#endif  // ifdef __cplusplus

#endif  // STARBOARD_FILE_H_

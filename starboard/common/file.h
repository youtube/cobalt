// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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
// Implements a set of convenience functions and macros built on top of the core
// Starboard file API.

#ifndef STARBOARD_COMMON_FILE_H_
#define STARBOARD_COMMON_FILE_H_

#include "starboard/file.h"

namespace starboard {

// Deletes the file, symlink or directory at |path|. When |path| is a directory,
// the function will recursively delete the entire tree; however, when
// |preserve_root| is |true| the root directory is not removed. On some
// platforms, this function fails if a file to be deleted is being held open.
//
// Returns |true| if the file, symlink, or directory was able to be deleted, and
// |false| if there was an error at any point.
//
// |path|: The absolute path of the file, symlink, or directory to be deleted.
// |preserve_root|: Whether or not the root directory should be preserved.
bool SbFileDeleteRecursive(const char* path, bool preserve_root);

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

  int ReadAll(char* data, int size) const {
    return SbFileReadAll(file_, data, size);
  }

  int Write(const char* data, int size) const {
    return SbFileWrite(file_, data, size);
  }

  int WriteAll(const char* data, int size) const {
    return SbFileWriteAll(file_, data, size);
  }

  bool Truncate(int64_t length) const { return SbFileTruncate(file_, length); }

  bool Flush() const { return SbFileFlush(file_); }

  bool GetInfo(SbFileInfo* out_info) const {
    return SbFileGetInfo(file_, out_info);
  }

  int64_t GetSize() const {
    SbFileInfo file_info;
    bool success = GetInfo(&file_info);
    return (success ? file_info.size : -1);
  }

  // disallow copy and move operations
  ScopedFile(const ScopedFile&) = delete;
  ScopedFile& operator=(const ScopedFile&) = delete;

 private:
  SbFile file_;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_FILE_H_

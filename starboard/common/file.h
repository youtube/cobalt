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

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "starboard/common/log.h"

namespace starboard {

bool FileCanOpen(const char* path, int flags);

bool IsValid(int file);

ssize_t ReadAll(int fd, void* data, int size);

void RecordFileWriteStat(int write_file_result);

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

ssize_t WriteAll(int fd, const void* data, int size);

// A class that opens an file descriptor in its constructor and closes it in its
// destructor, so the file is open for the lifetime of the object. Member
// functions call the corresponding file function.
class ScopedFile {
 public:
  ScopedFile(const char* path, int flags, int mode) : file_(-1) {
    file_ = open(path, flags, mode);
  }

  ScopedFile(const char* path, int flags) : file_(-1) {
    file_ = open(path, flags, S_IRUSR | S_IWUSR);
  }

  ~ScopedFile() {
    if (file_ >= 0) {
      close(file_);
    }
  }

  int file() const { return file_; }

  bool IsValid() const { return starboard::IsValid(file_); }

  int64_t Seek(int64_t offset, int whence) const {
    return lseek(file_, static_cast<off_t>(offset), whence);
  }

  int Read(char* data, int size) const { return read(file_, data, size); }

  int ReadAll(char* data, int size) const {
    return starboard::ReadAll(file_, data, size);
  }

  int Write(const char* data, int size) const {
    int result = write(file_, data, size);
    RecordFileWriteStat(result);
    return result;
  }

  int WriteAll(const char* data, int size) const {
    int result = starboard::WriteAll(file_, data, size);
    RecordFileWriteStat(result);
    return result;
  }

  bool Truncate(int64_t length) const { return !ftruncate(file_, length); }

  bool Flush() const { return !fsync(file_); }

  bool GetInfo(struct stat* out_info) const { return !fstat(file_, out_info); }

  int64_t GetSize() const {
    struct stat file_info;
    bool success = GetInfo(&file_info);
    return (success ? file_info.st_size : -1);
  }

  // disallow copy and move operations
  ScopedFile(const ScopedFile&) = delete;
  ScopedFile& operator=(const ScopedFile&) = delete;

 private:
  int file_;
};

}  // namespace starboard

#endif  // STARBOARD_COMMON_FILE_H_

/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Follows the interface defined in dir_reader_posix.h/dir_reader_fallback.h
// but isn't directly included as the memory usage of this object may be higher
// than that offered by the dir_reader_linux implementation. Therefore the
// programmer must ask for this DirReaderDirent implementation by name. Also
// we add a few methods to this object to make file enumeration more convenient.

#ifndef BASE_DIR_READER_DIRENT_H_
#define BASE_DIR_READER_DIRENT_H_
#pragma once

#include <dirent.h>

#include "base/logging.h"

namespace base {

class DirReaderDirent {
 public:
  explicit DirReaderDirent(const char* directory_path)
      : current_(NULL) {
    dir_ = opendir(directory_path);
  }

  ~DirReaderDirent() {
    if (dir_) {
      closedir(dir_);
    }
  }

  bool IsValid() const {
    return dir_ != NULL;
  }

  bool Next() {
    if (!IsValid()) return false;
    current_ = readdir(dir_);
    return current_ != NULL;
  }

  const char* name() const {
    if (!IsValid()) return NULL;
    if (!current_) return NULL;
    return current_->d_name;
  }

#if !defined(__LB_SHELL__)
  // reading the linux implementation this is clear it is a call for the
  // opened directory fd
  int fd() const {
    if (!IsValid()) return -1;
    return dir_->fd;
  }
#endif

  static bool IsFallback() {
    return false;
  }

  //**************** extra convenience methods

  // returns true if the current enumerated object is a file
  bool IsFile() const {
    if (!IsValid()) return false;
    if (!current_) return false;
    return (current_->d_type == DT_REG);
  }

 private:
  DIR* dir_;
  dirent* current_;
};

}
#endif  // BASE_DIR_READER_DIRENT_H_

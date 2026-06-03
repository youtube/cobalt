// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef BASE_FILES_DIR_READER_STARBOARD_H_
#define BASE_FILES_DIR_READER_STARBOARD_H_

#include <dirent.h>
#include <errno.h>

namespace base {

class DirReaderStarboard {
 public:
  explicit DirReaderStarboard(const char* directory_path)
      : dir_(opendir(directory_path)), entry_(nullptr) {}

  ~DirReaderStarboard() {
    if (dir_) {
      closedir(dir_);
    }
  }

  bool IsValid() const {
    return dir_ != nullptr;
  }

  bool Next() {
    if (!IsValid()) {
      return false;
    }
    errno = 0;
    entry_ = readdir(dir_);
    return entry_ != nullptr;
  }

  const char* name() {
    return entry_ ? entry_->d_name : nullptr;
  }

  int fd() const {
    if (!IsValid()) {
      return -1;
    }
    return dirfd(dir_);
  }

  static bool IsFallback() {
    return false;
  }

 private:
  DIR* dir_;
  struct dirent* entry_;
};

}  // namespace base

#endif  // BASE_FILES_DIR_READER_STARBOARD_H_

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

// Adapted from base/platform_file_posix.cc

#include "starboard/file.h"

#include <errno.h>
#include <sys/stat.h>

bool SbFileCanOpen(const char* path, int flags) {
  bool can_read = flags & kSbFileRead;
  bool can_write = flags & kSbFileWrite;
  if (!can_read && !can_write) {
    return false;
  }

  struct stat file_info;
  if (stat(path, &file_info) != 0) {
    return false;
  }

  if (can_read && !(file_info.st_mode & S_IRUSR)) {
    errno = EACCES;
    return false;
  }

  if (can_write && !(file_info.st_mode & S_IWUSR)) {
    errno = EACCES;
    return false;
  }

  return true;
}

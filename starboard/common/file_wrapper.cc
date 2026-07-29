// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/file_wrapper.h"

#include <fcntl.h>
#include <unistd.h>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace {

bool IsUpdate(const char* mode) {
  for (const char* m = mode; *m != '\0'; ++m) {
    if (*m == '+') {
      return true;
    }
  }

  return false;
}
}  // namespace

extern "C" {

int FileModeStringToFlags(const char* mode) {
  if (!mode) {
    return 0;
  }

  int length = static_cast<int>(strlen(mode));
  if (length < 1) {
    return 0;
  }

  int flags = 0;
  switch (mode[0]) {
    case 'r':
      if (IsUpdate(mode + 1)) {
        flags |= O_RDWR;
      } else {
        flags |= O_RDONLY;
      }
      break;
    case 'w':
      if (IsUpdate(mode + 1)) {
        flags |= O_RDWR;
      } else {
        flags |= O_WRONLY;
      }
      flags |= O_CREAT | O_TRUNC;
      break;
    case 'a':
      if (IsUpdate(mode + 1)) {
        flags |= O_RDWR;
      } else {
        flags |= O_WRONLY;
      }
      flags |= O_CREAT;
      break;
    default:
      SB_NOTREACHED();
      break;
  }
  return flags;
}

int file_close(FilePtr file) {
  if (!file) {
    return -1;
  }
  int result = -1;
  if (file->fd >= 0) {
    result = close(file->fd);
  }
  delete file;
  return result;
}

FilePtr file_open(const char* path, int mode) {
  FilePtr file = new FileStruct();
  file->fd = open(path, mode, S_IRUSR | S_IWUSR);
  return file;
}

}  // extern "C"

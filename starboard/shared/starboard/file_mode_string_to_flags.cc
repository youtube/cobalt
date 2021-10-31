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

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/file.h"

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

int SbFileModeStringToFlags(const char* mode) {
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
        flags |= kSbFileWrite;
      }
      flags |= kSbFileOpenOnly | kSbFileRead;
      break;
    case 'w':
      if (IsUpdate(mode + 1)) {
        flags |= kSbFileRead;
      }
      flags |= kSbFileCreateAlways | kSbFileWrite;
      break;
    case 'a':
      if (IsUpdate(mode + 1)) {
        flags |= kSbFileRead;
      }
      flags |= kSbFileOpenAlways | kSbFileWrite;
      break;
    default:
      SB_NOTREACHED();
      break;
  }
  return flags;
}

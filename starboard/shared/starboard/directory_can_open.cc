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
#if SB_API_VERSION < 16

#include "starboard/directory.h"

#include "starboard/file.h"

bool SbDirectoryCanOpen(const char* path) {
  if (!path || !path[0]) {
    return false;
  }

  SbFileInfo info;
  bool result = SbFileGetPathInfo(path, &info);
  if (!result) {
    return false;
  }

  return info.is_directory;
}
#endif  // SB_API_VERSION < 16

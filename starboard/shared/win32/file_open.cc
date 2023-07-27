// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/file.h"

#include "starboard/shared/win32/file_internal.h"

SbFile SbFileOpen(const char* path,
                  int flags,
                  bool* out_created,
                  SbFileError* out_error) {
  if ((path == nullptr) || (path[0] == '\0')) {
    if (out_created) {
      *out_created = false;
    }
    if (out_error) {
      *out_error = kSbFileErrorNotAFile;
    }
    return kSbFileInvalid;
  }

  HANDLE file_handle =
      starboard::shared::win32::OpenFileOrDirectory(path, flags, out_created, out_error);

  if (!starboard::shared::win32::IsValidHandle(file_handle)) {
    return kSbFileInvalid;
  }

  return new SbFilePrivate(file_handle);
}

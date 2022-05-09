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

#include <windows.h>

#include "starboard/common/log.h"
#include "starboard/shared/win32/file_internal.h"

int SbFileWrite(SbFile file, const char* data, int size) {
  // TODO: Support asynchronous IO, if necessary.
  SB_DCHECK((size == 0) || (data != nullptr));
  if (!SbFileIsValid(file)) {
    return -1;
  }
  if (size < 0) {
    SB_NOTREACHED();
    return -1;
  } else if (size == 0) {
    return 0;
  }

  DWORD bytes_written = 0;
  bool success = WriteFile(file->file_handle, data, size, &bytes_written, NULL);
  if (!success) {
    return -1;
  }

  return bytes_written;
}

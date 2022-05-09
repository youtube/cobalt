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

int SbFileRead(SbFile file, char* data, int size) {
  SB_DCHECK((size == 0) || (data != nullptr));

  if (!starboard::shared::win32::HasValidHandle(file)) {
    return -1;
  }

  if (size < 0) {
    SB_NOTREACHED();
    return -1;
  } else if (size == 0) {
    return 0;
  }

  DWORD number_bytes_read = 0;
  BOOL success =
      ReadFile(file->file_handle, data, size, &number_bytes_read, nullptr);

  // Since we are only doing synchronous IO, success == FALSE implies that
  // something is wrong.
  if (!success) {
    return -1;
  }

  return number_bytes_read;
}

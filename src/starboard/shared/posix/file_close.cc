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

#include <unistd.h>

#include "starboard/shared/posix/file_internal.h"
#include "starboard/shared/posix/handle_eintr.h"

bool SbFileClose(SbFile file) {
  if (!file) {
    return false;
  }

  bool result = false;
  if (file->descriptor >= 0) {
    result = !HANDLE_EINTR(close(file->descriptor));
  }

  delete file;

  return result;
}

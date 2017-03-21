// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_ISO_IMPL_DIRECTORY_CLOSE_H_
#define STARBOARD_SHARED_ISO_IMPL_DIRECTORY_CLOSE_H_

#include "starboard/directory.h"

#include <dirent.h>
#include <errno.h>

#include "starboard/file.h"

#include "starboard/shared/internal_only.h"
#include "starboard/shared/iso/impl/directory_impl.h"

namespace starboard {
namespace shared {
namespace iso {
namespace impl {

bool SbDirectoryClose(SbDirectory directory) {
  if (!directory || !directory->directory) {
    return false;
  }

  bool result = !closedir(directory->directory);
  delete directory;
  return result;
}

}  // namespace impl
}  // namespace iso
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_ISO_IMPL_DIRECTORY_CLOSE_H_

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

#include <android/asset_manager.h>

#include "starboard/directory.h"

#include "starboard/android/shared/file_internal.h"
#include "starboard/shared/posix/impl/file_can_open.h"

using starboard::android::shared::IsAndroidAssetPath;
using starboard::android::shared::OpenAndroidAsset;

bool SbFileCanOpen(const char* path, int flags) {
  if (!IsAndroidAssetPath(path)) {
    return ::starboard::shared::posix::impl::FileCanOpen(path, flags);
  }

  SbFile file = SbFileOpen(path, flags | kSbFileOpenOnly, NULL, NULL);
  bool result = SbFileIsValid(file);
  SbFileClose(file);

  if (!result) {
    SbDirectory directory = SbDirectoryOpen(path, NULL);
    result = SbDirectoryIsValid(directory);
    SbDirectoryClose(directory);
  }

  return result;
}

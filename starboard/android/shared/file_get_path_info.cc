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

#include "starboard/file.h"

#include "starboard/android/shared/file_internal.h"
#include "starboard/shared/posix/impl/file_get_path_info.h"

using starboard::android::shared::IsAndroidAssetPath;
using starboard::android::shared::OpenAndroidAsset;

bool SbFileGetPathInfo(const char* path, SbFileInfo* out_info) {
  if (!IsAndroidAssetPath(path)) {
    return ::starboard::shared::posix::impl::FileGetPathInfo(path, out_info);
  }

  bool result = false;
  SbFile file = SbFileOpen(path, kSbFileRead, NULL, NULL);
  if (file) {
    result = SbFileGetInfo(file, out_info);
    SbFileClose(file);
  }
  return result;
}

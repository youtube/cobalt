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

#include "starboard/directory.h"

#include <android/asset_manager.h>

#include "starboard/android/shared/file_internal.h"

#include "starboard/android/shared/directory_internal.h"
#include "starboard/shared/iso/impl/directory_open.h"

using starboard::android::shared::IsAndroidAssetPath;
using starboard::android::shared::OpenAndroidAssetDir;

SbDirectory SbDirectoryOpen(const char* path, SbFileError* out_error) {
  if (!IsAndroidAssetPath(path)) {
    return ::starboard::shared::iso::impl::SbDirectoryOpen(path, out_error);
  }

  AAssetDir* asset_dir = OpenAndroidAssetDir(path);
  if (asset_dir) {
    SbDirectory result = new SbDirectoryPrivate();
    result->asset_dir = asset_dir;
    return result;
  }

  if (out_error) {
    *out_error = kSbFileErrorFailed;
  }
  return kSbDirectoryInvalid;
}

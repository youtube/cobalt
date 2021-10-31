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

#include "starboard/android/shared/file_internal.h"
#include "starboard/shared/posix/impl/file_get_info.h"

bool SbFileGetInfo(SbFile file, SbFileInfo* out_info) {
  if (file && file->asset && out_info) {
    out_info->creation_time = 0;
    out_info->is_directory = 0;
    out_info->is_symbolic_link = 0;
    out_info->last_accessed = 0;
    out_info->last_modified = 0;
    out_info->size = AAsset_getLength(file->asset);
    return true;
  }

  return ::starboard::shared::posix::impl::FileGetInfo(file, out_info);
}

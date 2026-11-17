// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <android/asset_manager.h>

#include "starboard/android/shared/file_internal.h"
#include "starboard/common/log.h"
#include "starboard/elf_loader/file_open.h"

namespace elf_loader {

// Uncompressed assets are read straight from the APK. Opening an asset path
// with open() would copy the whole asset to a temporary file first (see
// AssetManager::Open), which for the Cobalt core costs most of the load time.
int OpenFileInPlace(const char* name, int64_t* offset, int64_t* size) {
  if (!starboard::IsAndroidAssetPath(name)) {
    return -1;
  }
  AAsset* asset = starboard::OpenAndroidAsset(name);
  if (!asset) {
    return -1;
  }
  off64_t start = 0;
  off64_t length = 0;
  int fd = AAsset_openFileDescriptor64(asset, &start, &length);
  AAsset_close(asset);
  if (fd < 0) {
    SB_LOG(INFO) << name << " is compressed in the APK, copying it.";
    return -1;
  }
  SB_LOG(INFO) << "Reading " << name << " from the APK at offset " << start
               << ", size " << length;
  *offset = start;
  *size = length;
  return fd;
}

}  // namespace elf_loader

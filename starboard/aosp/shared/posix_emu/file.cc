// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <fcntl.h>

#include <vector>

#include "starboard/android/shared/asset_manager.h"
#include "starboard/android/shared/file_internal.h"
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/log.h"

using starboard::android::shared::AssetManager;
using starboard::android::shared::IsAndroidAssetPath;
using starboard::android::shared::OpenAndroidAsset;

// ///////////////////////////////////////////////////////////////////////////////
// // Implementations below exposed externally in pure C for emulation.
// ///////////////////////////////////////////////////////////////////////////////

extern "C" {
int __real_close(int fildes);
int __real_open(const char* path, int oflag, ...);

int __wrap_close(int fildes) {
  AssetManager* asset_manager = AssetManager::GetInstance();
  if (asset_manager->IsAssetFd(fildes)) {
    return asset_manager->Close(fildes);
  }
  return __real_close(fildes);
}

int __wrap_open(const char* path, int oflag, ...) {
  if (!IsAndroidAssetPath(path)) {
    va_list args;
    va_start(args, oflag);
    int fd;
    if (oflag & O_CREAT) {
      mode_t mode = va_arg(args, int);
      return __real_open(path, oflag, mode);
    } else {
      return __real_open(path, oflag);
    }
  }
  return AssetManager::GetInstance()->Open(path, oflag);
}

}  // extern "C"

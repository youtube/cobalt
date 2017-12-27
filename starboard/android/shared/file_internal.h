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

#ifndef STARBOARD_ANDROID_SHARED_FILE_INTERNAL_H_
#define STARBOARD_ANDROID_SHARED_FILE_INTERNAL_H_

#include <errno.h>

#include <android/asset_manager.h>

#include "starboard/file.h"
#include "starboard/shared/internal_only.h"

struct SbFilePrivate {
  // Note: Only one of these two fields will be valid for any given file.

  // The POSIX file descriptor of this file, or -1 if it's a read-only asset.
  int descriptor;

  // If not NULL this is an Android asset.
  AAsset* asset;

  SbFilePrivate() : descriptor(-1), asset(NULL) {}
};

namespace starboard {
namespace android {
namespace shared {

extern const char* g_app_assets_dir;
extern const char* g_app_files_dir;
extern const char* g_app_cache_dir;

void SbFileAndroidInitialize();
void SbFileAndroidTeardown();

bool IsAndroidAssetPath(const char* path);
AAsset* OpenAndroidAsset(const char* path);
AAssetDir* OpenAndroidAssetDir(const char* path);

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_FILE_INTERNAL_H_

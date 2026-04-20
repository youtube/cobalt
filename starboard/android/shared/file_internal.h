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

#ifndef STARBOARD_ANDROID_SHARED_FILE_INTERNAL_H_
#define STARBOARD_ANDROID_SHARED_FILE_INTERNAL_H_

#include <android/asset_manager.h>
#include <errno.h>
#include <jni.h>

#include <string>

#include "starboard/android/shared/starboard_bridge.h"
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

extern const char* g_app_assets_dir;
extern const char* g_app_files_dir;
extern const char* g_app_cache_dir;
extern const char* g_app_lib_dir;

void SbFileAndroidInitialize(
    base::android::ScopedJavaGlobalRef<jobject> asset_manager,
    const std::string& files_dir,
    const std::string& cache_dir,
    const std::string& native_library_dir);
void SbFileAndroidTeardown();

bool IsAndroidAssetPath(const char* path);
AAsset* OpenAndroidAsset(const char* path);
AAssetDir* OpenAndroidAssetDir(const char* path);

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_FILE_INTERNAL_H_

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

#include "starboard/android/shared/file_internal.h"

#include <android/log.h>
#include <jni.h>
#include <string>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/string.h"

namespace starboard {
namespace android {
namespace shared {

const char* g_app_assets_dir = "/cobalt/assets";
const char* g_app_files_dir = NULL;
const char* g_app_cache_dir = NULL;

namespace {
AAssetManager* g_asset_manager;

// Copies the characters from a jstring and returns a newly allocated buffer
// with the result.
const char* DuplicateJavaString(JniEnvExt* env, jstring j_string) {
  SB_DCHECK(j_string);
  std::string utf_str = env->GetStringStandardUTFOrAbort(j_string);
  const char* result = SbStringDuplicate(utf_str.c_str());
  return result;
}

}  // namespace

void SbFileAndroidInitialize(ANativeActivity* activity) {
  SB_DCHECK(g_asset_manager == NULL);
  g_asset_manager = activity->assetManager;

  JniEnvExt* env = JniEnvExt::Get();

  SB_DCHECK(g_app_files_dir == NULL);
  ScopedLocalJavaRef<jstring> j_string(
      env->CallStarboardObjectMethodOrAbort("getFilesAbsolutePath",
                                           "()Ljava/lang/String;"));
  g_app_files_dir = DuplicateJavaString(env, j_string.Get());
  SB_DLOG(INFO) << "Files dir: " << g_app_files_dir;

  SB_DCHECK(g_app_cache_dir == NULL);
  j_string.Reset(
      env->CallStarboardObjectMethodOrAbort("getCacheAbsolutePath",
                                           "()Ljava/lang/String;"));
  g_app_cache_dir = DuplicateJavaString(env, j_string.Get());
  SB_DLOG(INFO) << "Cache dir: " << g_app_cache_dir;
}

void SbFileAndroidTeardown() {
  g_asset_manager = NULL;

  if (g_app_files_dir) {
    SbMemoryDeallocate(const_cast<char*>(g_app_files_dir));
    g_app_files_dir = NULL;
  }

  if (g_app_cache_dir) {
    SbMemoryDeallocate(const_cast<char*>(g_app_cache_dir));
    g_app_cache_dir = NULL;
  }
}

bool IsAndroidAssetPath(const char* path) {
  size_t prefix_len = SbStringGetLength(g_app_assets_dir);
  return path != NULL
      && SbStringCompare(g_app_assets_dir, path, prefix_len) == 0
      && (path[prefix_len] == '/' || path[prefix_len] == '\0');
}

AAsset* OpenAndroidAsset(const char* path) {
  if (!IsAndroidAssetPath(path) || g_asset_manager == NULL) {
    return NULL;
  }
  const char* asset_path = path + SbStringGetLength(g_app_assets_dir) + 1;
  return AAssetManager_open(g_asset_manager, asset_path, AASSET_MODE_RANDOM);
}

AAssetDir* OpenAndroidAssetDir(const char* path) {
  if (!IsAndroidAssetPath(path) || g_asset_manager == NULL) {
    return NULL;
  }
  const char* asset_path = path + SbStringGetLength(g_app_assets_dir);
  if (*asset_path == '/') {
    asset_path++;
  }
  return AAssetManager_openDir(g_asset_manager, asset_path);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

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

#include "starboard/android/shared/file_internal.h"

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <jni.h>
#include <string>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/memory.h"

namespace starboard {
namespace android {
namespace shared {

const char* g_app_assets_dir = "/cobalt/assets";
const char* g_app_files_dir = NULL;
const char* g_app_cache_dir = NULL;
const char* g_app_lib_dir = NULL;

namespace {
jobject g_java_asset_manager;
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

void SbFileAndroidInitialize() {
  JniEnvExt* env = JniEnvExt::Get();

  SB_DCHECK(g_java_asset_manager == NULL);
  SB_DCHECK(g_asset_manager == NULL);
  ScopedLocalJavaRef<jstring> j_app(
      env->CallStarboardObjectMethodOrAbort(
          "getApplicationContext", "()Landroid/content/Context;"));
  g_java_asset_manager = env->ConvertLocalRefToGlobalRef(
      env->CallObjectMethodOrAbort(j_app.Get(),
          "getAssets", "()Landroid/content/res/AssetManager;"));
  g_asset_manager = AAssetManager_fromJava(env, g_java_asset_manager);

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

  SB_DCHECK(g_app_lib_dir == NULL);
  ScopedLocalJavaRef<jobject> j_app_info(
      env->CallObjectMethodOrAbort(j_app.Get(), "getApplicationInfo",
                                   "()Landroid/content/pm/ApplicationInfo;"));
  j_string.Reset(env->GetStringFieldOrAbort(j_app_info.Get(),
                                            "nativeLibraryDir"));
  g_app_lib_dir = DuplicateJavaString(env, j_string.Get());
  SB_DLOG(INFO) << "Lib dir: " << g_app_lib_dir;
}

void SbFileAndroidTeardown() {
  JniEnvExt* env = JniEnvExt::Get();

  if (g_java_asset_manager) {
    env->DeleteGlobalRef(g_java_asset_manager);
    g_java_asset_manager = NULL;
    g_asset_manager = NULL;
  }

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
  size_t prefix_len = strlen(g_app_assets_dir);
  return path != NULL
      && strncmp(g_app_assets_dir, path, prefix_len) == 0
      && (path[prefix_len] == '/' || path[prefix_len] == '\0');
}

AAsset* OpenAndroidAsset(const char* path) {
  if (!IsAndroidAssetPath(path) || g_asset_manager == NULL) {
    return NULL;
  }
  const char* asset_path = path + strlen(g_app_assets_dir) + 1;
  return AAssetManager_open(g_asset_manager, asset_path, AASSET_MODE_RANDOM);
}

AAssetDir* OpenAndroidAssetDir(const char* path) {
  // Note that the asset directory will not be found if |path| points to a
  // specific file instead of a directory. |path| should also not end with '/'.
  if (!IsAndroidAssetPath(path) || g_asset_manager == NULL) {
    return NULL;
  }
  const char* asset_path = path + strlen(g_app_assets_dir);
  if (*asset_path == '/') {
    asset_path++;
  }
  AAssetDir* asset_directory =
      AAssetManager_openDir(g_asset_manager, asset_path);

  // AAssetManager_openDir() always returns a pointer to an initialized object,
  // even if the given directory does not exist. To determine if the directory
  // actually exists, AAssetDir_getNextFileName() is called to check for any
  // files in the given directory. However, when iterating the contents of a
  // directory, the NDK Asset Manager does not allow subdirectories to be seen.
  // So this is not a 100% accurate way to determine if a directory actually
  // exists and false negatives will be given for directories that contain
  // subdirectories but no files in their immediate directory.
  const char* file = AAssetDir_getNextFileName(asset_directory);
  if (file == NULL) {
    AAssetDir_close(asset_directory);
    return NULL;
  } else {
    AAssetDir_rewind(asset_directory);
    return asset_directory;
  }
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

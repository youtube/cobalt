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

#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace starboard::android::shared {

const char* g_app_assets_dir = "/cobalt/assets";
const char* g_app_files_dir = NULL;
const char* g_app_cache_dir = NULL;
const char* g_app_lib_dir = NULL;

namespace {
ScopedJavaGlobalRef<jobject> g_java_asset_manager;
AAssetManager* g_asset_manager;
}  // namespace

void SbFileAndroidInitialize() {
  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge* starbooard_bridge = StarboardBridge::GetInstance();

  SB_DCHECK(g_java_asset_manager.is_null());
  SB_DCHECK(g_asset_manager == NULL);

  ScopedJavaLocalRef<jobject> context =
      starbooard_bridge->GetApplicationContext(env);

  g_java_asset_manager = starbooard_bridge->GetAssetsFromContext(env, context);
  g_asset_manager = AAssetManager_fromJava(env, g_java_asset_manager.obj());

  std::string app_files_dir = starbooard_bridge->GetFilesAbsolutePath(env);
  SB_DCHECK(g_app_files_dir == NULL);
  g_app_files_dir = strdup(app_files_dir.c_str());

  SB_DLOG(INFO) << "Files dir: " << g_app_files_dir;

  std::string app_cache_dir = starbooard_bridge->GetCacheAbsolutePath(env);
  SB_DCHECK(g_app_cache_dir == NULL);
  g_app_cache_dir = strdup(app_cache_dir.c_str());

  SB_DLOG(INFO) << "Cache dir: " << g_app_cache_dir;

  std::string app_lib_dir =
      starbooard_bridge->GetNativeLibraryDirFromContext(env, context);
  SB_DCHECK(g_app_lib_dir == NULL);
  g_app_lib_dir = strdup(app_lib_dir.c_str());

  SB_DLOG(INFO) << "Lib dir: " << g_app_lib_dir;
}

void SbFileAndroidTeardown() {
  if (g_java_asset_manager) {
    g_java_asset_manager.Reset();
    g_asset_manager = NULL;
  }

  if (g_app_files_dir) {
    free(const_cast<char*>(g_app_files_dir));
    g_app_files_dir = NULL;
  }

  if (g_app_cache_dir) {
    free(const_cast<char*>(g_app_cache_dir));
    g_app_cache_dir = NULL;
  }
}

bool IsAndroidAssetPath(const char* path) {
  size_t prefix_len = strlen(g_app_assets_dir);
  return path != NULL && strncmp(g_app_assets_dir, path, prefix_len) == 0 &&
         (path[prefix_len] == '/' || path[prefix_len] == '\0');
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

}  // namespace starboard::android::shared

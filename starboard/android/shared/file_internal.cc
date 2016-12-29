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

// Makes a JNI call to File.getAbsolutePath() and returns a newly allocated
// buffer with the result.
const char* GetAbsolutePath(JNIEnv* env, jobject file_obj) {
  SB_DCHECK(file_obj);
  jclass file_class = env->GetObjectClass(file_obj);
  jmethodID method = env->GetMethodID(file_class,
      "getAbsolutePath", "()Ljava/lang/String;");
  jstring abs_path = (jstring) (env->CallObjectMethod(file_obj, method));
  const char* utf_chars = env->GetStringUTFChars(abs_path, 0);
  const char* result = SbStringDuplicate(utf_chars);
  env->ReleaseStringUTFChars(abs_path, utf_chars);
  return result;
}

}  // namespace

void SbFileAndroidInitialize(ANativeActivity* activity) {
  // TODO: Make an APK test runner so we don't need this
  if (activity == NULL) {
    SB_LOG(ERROR) << "NULL Activity; Initializing fake file paths for tests";
    g_asset_manager = NULL;
    g_app_files_dir = SbStringDuplicate("/data/local/tmp/cobalt");
    g_app_cache_dir = SbStringDuplicate("/data/local/tmp/cobalt");
    return;
  }

  SB_DCHECK(g_asset_manager == NULL);
  g_asset_manager = activity->assetManager;

  JNIEnv* env;
  activity->vm->AttachCurrentThread(&env, 0);

  jobject activity_obj = activity->clazz;
  jclass activity_class = env->GetObjectClass(activity_obj);

  SB_DCHECK(g_app_files_dir == NULL);
  jmethodID method = env->GetMethodID(activity_class,
      "getFilesDir", "()Ljava/io/File;");
  jobject file_obj = env->CallObjectMethod(activity_obj, method);
  g_app_files_dir = GetAbsolutePath(env, file_obj);
  SB_LOG(INFO) << "Files dir: " << g_app_files_dir;

  SB_DCHECK(g_app_cache_dir == NULL);
  method = env->GetMethodID(activity_class,
      "getCacheDir", "()Ljava/io/File;");
  file_obj = env->CallObjectMethod(activity_obj, method);
  g_app_cache_dir = GetAbsolutePath(env, file_obj);
  SB_LOG(INFO) << "Cache dir: " << g_app_cache_dir;

  activity->vm->DetachCurrentThread();
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
  size_t prefix_len = strlen(g_app_assets_dir);
  return path != NULL
      && strncmp(g_app_assets_dir, path, prefix_len) == 0
      && path[prefix_len] == '/';
}

AAsset* OpenAndroidAsset(const char* path) {
  if (!IsAndroidAssetPath(path) || g_asset_manager == NULL) {
    return NULL;
  }
  const char* asset_path = path + strlen(g_app_assets_dir) + 1;
  return AAssetManager_open(g_asset_manager, asset_path, AASSET_MODE_RANDOM);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

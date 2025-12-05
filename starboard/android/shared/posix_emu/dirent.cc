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

#include <android/asset_manager.h>
#include <dirent.h>

#include <map>
#include <mutex>

#include "starboard/android/shared/file_internal.h"
#include "starboard/common/string.h"
#include "starboard/configuration_constants.h"

using starboard::IsAndroidAssetPath;
using starboard::OpenAndroidAsset;
using starboard::OpenAndroidAssetDir;

///////////////////////////////////////////////////////////////////////////////
// Implementations below exposed externally in pure C for emulation.
///////////////////////////////////////////////////////////////////////////////

extern "C" {
DIR* __real_opendir(const char* path);

int __real_closedir(DIR* dir);

int __real_readdir_r(DIR* __restrict dir,
                     struct dirent* __restrict dirent_buf,
                     struct dirent** __restrict dirent);

// AAssetDir does not have a file descriptor so we must generate one
// https://android.googlesource.com/platform/frameworks/base/+/master/native/android/asset_manager.cpp
static int gen_fd() {
  static int fd = 100;
  fd++;
  if (fd == 0x7FFFFFFF) {
    fd = 100;
  }
  return fd;
}

std::mutex mutex_;
static std::map<int, AAssetDir*>* asset_map = nullptr;

static int handle_db_put(AAssetDir* assetDir) {
  std::lock_guard scoped_lock(mutex_);
  if (asset_map == nullptr) {
    asset_map = new std::map<int, AAssetDir*>();
  }

  int fd = gen_fd();
  // Go through the map and make sure there isn't duplicated index
  // already.
  while (asset_map->find(fd) != asset_map->end()) {
    fd = gen_fd();
  }
  asset_map->insert({fd, assetDir});

  return fd;
}

static AAssetDir* handle_db_get(int fd, bool erase) {
  std::lock_guard scoped_lock(mutex_);
  if (fd < 0) {
    return nullptr;
  }
  if (asset_map == nullptr) {
    return nullptr;
  }

  auto itr = asset_map->find(fd);
  if (itr == asset_map->end()) {
    return nullptr;
  }

  AAssetDir* asset_dir = itr->second;
  if (erase) {
    asset_map->erase(itr);
  }
  return asset_dir;
}

struct _PosixEmuAndroidAssetDir {
  int64_t magic;
  int fd;
};

constexpr int64_t kMagicNum = -1;

DIR* __wrap_opendir(const char* path) {
  if (!IsAndroidAssetPath(path)) {
    return __real_opendir(path);
  }

  AAssetDir* asset_dir = OpenAndroidAssetDir(path);
  if (asset_dir) {
    int descriptor = handle_db_put(asset_dir);
    struct _PosixEmuAndroidAssetDir* retdir =
        reinterpret_cast<_PosixEmuAndroidAssetDir*>(
            malloc(sizeof(struct _PosixEmuAndroidAssetDir)));
    retdir->magic = kMagicNum;
    retdir->fd = descriptor;
    return reinterpret_cast<DIR*>(retdir);
  }
  return NULL;
}

int __wrap_closedir(DIR* dir) {
  if (!dir) {
    return -1;
  }
  struct _PosixEmuAndroidAssetDir* adir = (struct _PosixEmuAndroidAssetDir*)dir;
  if (adir->magic == kMagicNum) {  // This is an Asset
    int descriptor = adir->fd;
    AAssetDir* asset_dir = handle_db_get(descriptor, true);
    if (asset_dir != nullptr) {
      AAssetDir_close(asset_dir);
    }
    free(adir);
    return 0;
  }
  return __real_closedir(dir);
}

int __wrap_readdir_r(DIR* __restrict dir,
                     struct dirent* __restrict dirent_buf,
                     struct dirent** __restrict dirent) {
  if (!dir) {
    return -1;
  }

  struct _PosixEmuAndroidAssetDir* adir = (struct _PosixEmuAndroidAssetDir*)dir;
  if (adir->magic == kMagicNum) {  // This is an Asset
    int descriptor = adir->fd;
    AAssetDir* asset_dir = handle_db_get(descriptor, false);
    const char* file_name = AAssetDir_getNextFileName(asset_dir);
    if (file_name == NULL) {
      return -1;
    }
    *dirent = dirent_buf;
    starboard::strlcpy((*dirent)->d_name, file_name, kSbFileMaxName);
    return 0;
  }

  return __real_readdir_r(dir, dirent_buf, dirent);
}

}  // extern "C"

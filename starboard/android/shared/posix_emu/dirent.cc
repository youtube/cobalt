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

// clang-format off
#include <dirent.h>
// clang-format on

#include <errno.h>
#include <string.h>

#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "starboard/android/shared/asset_manager.h"
#include "starboard/android/shared/file_internal.h"
#include "starboard/common/string.h"

using starboard::AssetManager;
using starboard::IsAndroidAssetPath;
using starboard::ListAndroidAssetDir;

namespace {

// A helper structure used internally as a DIR replacement for asset paths.
// It's used in the wrapped opendir/fdopendir/closedir/readdir/readdir_r.
struct AndroidAssetDir {
  std::vector<std::string> entries;
  size_t index;
  struct dirent entry;
};

// Registry of the DIR* handles created by the asset-path wrappers below.
std::mutex& GetAssetDirMutex() {
  static std::mutex* mutex = new std::mutex();
  return *mutex;
}

std::set<const AndroidAssetDir*>& GetActiveAssetDirs() {
  static std::set<const AndroidAssetDir*>* dirs =
      new std::set<const AndroidAssetDir*>();
  return *dirs;
}

void RegisterAssetDir(const AndroidAssetDir* dir) {
  std::lock_guard lock(GetAssetDirMutex());
  GetActiveAssetDirs().insert(dir);
}

bool UnregisterAssetDir(const AndroidAssetDir* dir) {
  std::lock_guard lock(GetAssetDirMutex());
  return GetActiveAssetDirs().erase(dir) > 0;
}

bool IsAssetDir(const AndroidAssetDir* dir) {
  std::lock_guard lock(GetAssetDirMutex());
  return GetActiveAssetDirs().count(dir) > 0;
}

void FillAssetDirent(const std::string& name, struct dirent* out) {
  memset(out, 0, sizeof(*out));
  starboard::strlcpy(out->d_name, name.c_str(), sizeof(out->d_name));
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// Implementations below exposed externally in pure C for emulation.
///////////////////////////////////////////////////////////////////////////////

extern "C" {
DIR* __real_opendir(const char* path);

DIR* __real_fdopendir(int fd);

int __real_closedir(DIR* dir);

struct dirent* __real_readdir(DIR* dir);

int __real_readdir_r(DIR* __restrict dir,
                     struct dirent* __restrict dirent_buf,
                     struct dirent** __restrict dirent);

DIR* __wrap_opendir(const char* path) {
  if (!IsAndroidAssetPath(path)) {
    return __real_opendir(path);
  }

  std::vector<std::string> entries = ListAndroidAssetDir(path);
  if (entries.empty()) {
    errno = ENOENT;
    return NULL;
  }

  AndroidAssetDir* retdir = new AndroidAssetDir();
  retdir->entries = std::move(entries);
  retdir->index = 0;
  RegisterAssetDir(retdir);
  return reinterpret_cast<DIR*>(retdir);
}

DIR* __wrap_fdopendir(int fd) {
  AssetManager* asset_manager = AssetManager::GetInstance();
  if (!asset_manager->IsAssetDirFd(fd)) {
    return __real_fdopendir(fd);
  }

  std::vector<std::string> entries;
  asset_manager->GetDirectoryEntries(fd, &entries);
  // The reserved fd is only a placeholder used as a key, it can be
  // closed now that the entries have been already captured.
  asset_manager->CloseDirectory(fd);

  AndroidAssetDir* retdir = new AndroidAssetDir();
  retdir->entries = std::move(entries);
  retdir->index = 0;
  RegisterAssetDir(retdir);
  return reinterpret_cast<DIR*>(retdir);
}

int __wrap_closedir(DIR* dir) {
  if (!dir) {
    return -1;
  }
  AndroidAssetDir* asset_dir = reinterpret_cast<AndroidAssetDir*>(dir);
  if (UnregisterAssetDir(asset_dir)) {
    delete asset_dir;
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

  AndroidAssetDir* asset_dir = reinterpret_cast<AndroidAssetDir*>(dir);
  if (IsAssetDir(asset_dir)) {
    if (asset_dir->index >= asset_dir->entries.size()) {
      *dirent = NULL;  // End of directory
      return 0;
    }
    FillAssetDirent(asset_dir->entries[asset_dir->index++], dirent_buf);
    *dirent = dirent_buf;
    return 0;
  }

  return __real_readdir_r(dir, dirent_buf, dirent);
}

struct dirent* __wrap_readdir(DIR* dir) {
  if (!dir) {
    errno = EBADF;
    return NULL;
  }

  AndroidAssetDir* asset_dir = reinterpret_cast<AndroidAssetDir*>(dir);
  if (IsAssetDir(asset_dir)) {
    if (asset_dir->index >= asset_dir->entries.size()) {
      return NULL;
    }
    FillAssetDirent(asset_dir->entries[asset_dir->index++], &asset_dir->entry);
    return &asset_dir->entry;
  }

  return __real_readdir(dir);
}

}  // extern "C"

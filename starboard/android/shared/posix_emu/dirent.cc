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

#include <string>
#include <utility>
#include <vector>

#include "starboard/android/shared/asset_manager.h"
#include "starboard/android/shared/file_internal.h"
#include "starboard/common/string.h"

using starboard::AssetManager;
using starboard::IsAndroidAssetPath;

namespace {

// A helper structure used internally as a DIR replacement for asset paths.
// It's used in the wrapped opendir/fdopendir/closedir/readdir/readdir_r.
struct AndroidAssetDir {
  std::vector<std::string> entries;
  size_t index;
  struct dirent entry;
  // The AssetManager directory descriptor this handle owns. It is only a
  // placeholder used as a map key, but AssetManager::OpenDirectory() reserves a
  // real descriptor, so it stays open until __wrap_closedir() releases it.
  int fd;
};

// Wraps an AssetManager directory descriptor in the DIR handle from
// __wrap_opendir()/__wrap_fdopendir().
DIR* MakeAssetDir(AssetManager* asset_manager, int fd) {
  std::vector<std::string> entries;
  if (!asset_manager->GetDirectoryEntries(fd, &entries)) {
    // fd stopped being an asset directory between the caller's check and
    // this point, so nothing to enumerate and nothing to close.
    errno = EBADF;
    return NULL;
  }

  AndroidAssetDir* retdir = new AndroidAssetDir();
  retdir->entries = std::move(entries);
  retdir->index = 0;
  retdir->fd = fd;
  DIR* dir = reinterpret_cast<DIR*>(retdir);
  asset_manager->RegisterAssetDir(dir);
  return dir;
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

  AssetManager* asset_manager = AssetManager::GetInstance();
  // OpenDirectory() sets errno to ENOTDIR or ENOENT on failure.
  int fd = asset_manager->OpenDirectory(path);
  if (fd < 0) {
    return NULL;
  }
  return MakeAssetDir(asset_manager, fd);
}

DIR* __wrap_fdopendir(int fd) {
  AssetManager* asset_manager = AssetManager::GetInstance();
  if (!asset_manager->IsAssetDirFd(fd)) {
    return __real_fdopendir(fd);
  }

  return MakeAssetDir(asset_manager, fd);
}

int __wrap_closedir(DIR* dir) {
  if (!dir) {
    return -1;
  }
  AssetManager* asset_manager = AssetManager::GetInstance();
  if (asset_manager->UnregisterAssetDir(dir)) {
    AndroidAssetDir* asset_dir = reinterpret_cast<AndroidAssetDir*>(dir);
    int result = asset_manager->CloseDirectory(asset_dir->fd);
    delete asset_dir;
    return result;
  }
  return __real_closedir(dir);
}

int __wrap_readdir_r(DIR* __restrict dir,
                     struct dirent* __restrict dirent_buf,
                     struct dirent** __restrict dirent) {
  if (!dir) {
    // readdir_r() reports failure by returning an error number, not -1.
    return EBADF;
  }

  if (AssetManager::GetInstance()->IsAssetDir(dir)) {
    AndroidAssetDir* asset_dir = reinterpret_cast<AndroidAssetDir*>(dir);
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

  if (AssetManager::GetInstance()->IsAssetDir(dir)) {
    AndroidAssetDir* asset_dir = reinterpret_cast<AndroidAssetDir*>(dir);
    if (asset_dir->index >= asset_dir->entries.size()) {
      return NULL;
    }
    FillAssetDirent(asset_dir->entries[asset_dir->index++], &asset_dir->entry);
    return &asset_dir->entry;
  }

  return __real_readdir(dir);
}

}  // extern "C"

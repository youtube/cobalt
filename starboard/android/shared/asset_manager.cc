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

#include "starboard/android/shared/asset_manager.h"

#include <android/asset_manager.h>
#include <fcntl.h>
#include <ftw.h>
#include <linux/limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "starboard/android/shared/file_internal.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/once.h"
#include "starboard/common/string.h"
#include "starboard/system.h"

namespace starboard {
namespace android {
namespace shared {

// static
SB_ONCE_INITIALIZE_FUNCTION(AssetManager, AssetManager::GetInstance);

AssetManager::AssetManager() {
  const int kPathSize = PATH_MAX / 2;
  char path[kPathSize] = {0};
  SB_CHECK(SbSystemGetPath(kSbSystemPathTempDirectory, path, kPathSize))
      << "Unable to get system temp path for AssetManager.";
  SB_CHECK(starboard::strlcat(path, "/asset_tmp", kPathSize) < kPathSize)
      << "Unable to construct temp path for AssetManager.";
  tmp_root_ = path;
  ClearTempDir();
}

uint64_t AssetManager::AcquireInternalFd() {
  ScopedLock scoped_lock(mutex_);
  do {
    ++internal_fd_;
  } while (in_use_internal_fd_set_.count(internal_fd_) == 1);
  in_use_internal_fd_set_.insert(internal_fd_);
  return internal_fd_;
}

std::string AssetManager::TempFilepath(uint64_t internal_fd) const {
  return tmp_root_ + "/" + std::to_string(internal_fd);
}

int AssetManager::Open(const char* path) {
  if (!path) {
    return -1;
  }

  AAsset* asset = OpenAndroidAsset(path);
  if (!asset) {
    return -1;
  }

  // Create temporary POSIX file for the asset
  uint64_t internal_fd = AcquireInternalFd();
  std::string filepath = TempFilepath(internal_fd);
  int fd = open(filepath.c_str(), O_RDWR | O_TRUNC);
  if (fd < 0) {
    SB_LOG(WARNING) << "Failed to create temporary file for asset: " << path;
    mutex_.Acquire();
    in_use_internal_fd_set_.erase(internal_fd);
    mutex_.Release();
    return -1;
  }

  // Copy contents of asset into temporary file and then seek to start of file.
  const off_t size = AAsset_getLength(asset);
  const void* const data = AAsset_getBuffer(asset);
  if (write(fd, data, size) != size || lseek(fd, 0, SEEK_SET) != 0) {
    SB_LOG(WARNING) << "Failed to write temporary file for asset: " << path;
    mutex_.Acquire();
    in_use_internal_fd_set_.erase(internal_fd);
    mutex_.Release();  // Can't hold lock when calling close();
    close(fd);
    return -1;
  }
  AAsset_close(asset);

  // Keep track of the internal fd so we can delete its file on close();
  mutex_.Acquire();
  fd_to_internal_fd_map_[fd] = internal_fd;
  mutex_.Release();

  return fd;
}

bool AssetManager::IsAssetFd(int fd) const {
  ScopedLock scoped_lock(mutex_);
  return fd_to_internal_fd_map_.count(fd) == 1;
}

int AssetManager::Close(int fd) {
  mutex_.Acquire();
  if (auto search = fd_to_internal_fd_map_.find(fd);
      search != fd_to_internal_fd_map_.end()) {
    uint64_t internal_fd = search->second;
    fd_to_internal_fd_map_.erase(search);
    in_use_internal_fd_set_.erase(internal_fd);
    mutex_.Release();  // Can't hold lock when calling close();
    int retval = close(fd);
    std::string filepath = TempFilepath(internal_fd);
    if (unlink(filepath.c_str()) != 0) {
      SB_LOG(WARNING) << "Failed to delete temporary file: " << filepath;
    }
    return retval;
  }
  mutex_.Release();
  return -1;
}

void AssetManager::ClearTempDir() {
  auto callback = [](const char* child, const struct stat*, int file_type,
                     struct FTW*) -> int {
    SB_LOG(INFO) << "GARO: deleting " << child;
    return remove(child);
  };
  nftw(tmp_root_.c_str(), callback, 32, FTW_DEPTH | FTW_PHYS);
  mkdir(tmp_root_.c_str(), 0700);
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

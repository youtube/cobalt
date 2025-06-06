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
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <linux/limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <mutex>

#include "starboard/android/shared/file_internal.h"
#include "starboard/common/log.h"
#include "starboard/common/once.h"
#include "starboard/common/string.h"
#include "starboard/system.h"

namespace starboard::android::shared {

namespace {

// Returns the fallback for the given asset path, or an empty string if none.
// NOTE: While Cobalt now provides a mechanism for loading system fonts through
//       SbSystemGetPath(), using the fallback logic within SbFileOpen() is
//       still preferred for Android's fonts. The reason for this is that the
//       Android OS actually allows fonts to be loaded from two locations: one
//       that it provides; and one that the devices running its OS, which it
//       calls vendors, can provide. Rather than including the full Android font
//       package, vendors have the option of using a smaller Android font
//       package and supplementing it with their own fonts.
//
//       If Android were to use SbSystemGetPath() for its fonts, vendors would
//       have no way of providing those supplemental fonts to Cobalt, which
//       could result in a limited selection of fonts being available. By
//       treating Android's fonts as Cobalt's fonts, Cobalt can still offer a
//       straightforward mechanism for including vendor fonts via
//       SbSystemGetPath().
std::string FallbackPath(const std::string& path) {
  // We don't package most font files in Cobalt content and fallback to the
  // system font file of the same name.
  const std::string fonts_xml("fonts.xml");
  const std::string system_fonts_dir("/system/fonts/");
  const std::string cobalt_fonts_dir("/cobalt/assets/fonts/");

  // Fonts fallback to the system fonts.
  if (path.compare(0, cobalt_fonts_dir.length(), cobalt_fonts_dir) == 0) {
    std::string file_name = path.substr(cobalt_fonts_dir.length());
    // fonts.xml doesn't fallback.
    if (file_name != fonts_xml) {
      return system_fonts_dir + file_name;
    }
  }
  return std::string();
}

}  // namespace

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
  std::scoped_lock scoped_lock(mutex_);
  do {
    ++internal_fd_;
  } while (in_use_internal_fd_set_.count(internal_fd_) == 1);
  in_use_internal_fd_set_.insert(internal_fd_);
  return internal_fd_;
}

std::string AssetManager::TempFilepath(uint64_t internal_fd) const {
  return tmp_root_ + "/" + std::to_string(internal_fd);
}

int AssetManager::Open(const char* path, int oflag) {
  if (!path) {
    return -1;
  }

  AAsset* asset = OpenAndroidAsset(path);
  if (!asset) {
    std::string fallback_path = FallbackPath(path);
    if (!fallback_path.empty()) {
      return open(fallback_path.c_str(), oflag);
    }
    SB_LOG(WARNING) << "Asset path not found within package: " << path;
    return -1;
  }

  // Create temporary POSIX file for the asset
  uint64_t internal_fd = AcquireInternalFd();
  std::string filepath = TempFilepath(internal_fd);
  int fd =
      open(filepath.c_str(), O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0) {
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
  std::scoped_lock scoped_lock(mutex_);
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
                     struct FTW*) -> int { return remove(child); };
  nftw(tmp_root_.c_str(), callback, 32, FTW_DEPTH | FTW_PHYS);
  mkdir(tmp_root_.c_str(), 0700);
}

}  // namespace starboard::android::shared

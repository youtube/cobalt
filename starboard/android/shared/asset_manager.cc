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
#include <utility>
#include <vector>

#include "starboard/android/shared/file_internal.h"
#include "starboard/common/check_op.h"
#include "starboard/common/log.h"
#include "starboard/common/once.h"
#include "starboard/common/string.h"
#include "starboard/system.h"

namespace starboard {

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
SB_ONCE_INITIALIZE_FUNCTION(AssetManager, AssetManager::GetInstance)

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
    std::lock_guard lock(mutex_);
    in_use_internal_fd_set_.erase(internal_fd);
    return -1;
  }

  // Copy contents of asset into temporary file and then seek to start of file.
  const off_t size = AAsset_getLength(asset);
  const void* const data = AAsset_getBuffer(asset);
  if (write(fd, data, size) != size || lseek(fd, 0, SEEK_SET) != 0) {
    SB_LOG(WARNING) << "Failed to write temporary file for asset: " << path;
    {
      std::lock_guard lock(mutex_);
      in_use_internal_fd_set_.erase(internal_fd);
    }  // Can't hold lock when calling close();
    close(fd);
    return -1;
  }
  AAsset_close(asset);

  // Keep track of the internal fd so we can delete its file on close();
  {
    std::lock_guard scoped_lock(mutex_);
    fd_to_internal_fd_map_[fd] = internal_fd;
  }
  return fd;
}

int AssetManager::Close(int fd) {
  std::unique_lock lock(mutex_);
  if (auto search = fd_to_internal_fd_map_.find(fd);
      search != fd_to_internal_fd_map_.end()) {
    uint64_t internal_fd = search->second;
    fd_to_internal_fd_map_.erase(search);
    in_use_internal_fd_set_.erase(internal_fd);
    lock.unlock();  // Can't hold lock when calling close();
    int retval = close(fd);
    std::string filepath = TempFilepath(internal_fd);
    if (unlink(filepath.c_str()) != 0) {
      SB_LOG(WARNING) << "Failed to delete temporary file: " << filepath;
    }
    return retval;
  }
  return -1;
}

bool AssetManager::IsAssetFd(int fd) const {
  std::lock_guard scoped_lock(mutex_);
  return fd_to_internal_fd_map_.count(fd) == 1;
}

int AssetManager::OpenDirectory(const char* path) {
  if (!path) {
    return -1;
  }

  std::vector<std::string> entries = ListAndroidAssetDir(path);
  if (entries.empty()) {
    errno = ENOENT;
    return -1;
  }

  // Reserve a real, unique fd so it can be closed like any other fd.
  // It is never read but serves as a key to hold the directory entries
  // and the open/closed state.
  int fd = open("/dev/null", O_RDONLY);
  if (fd < 0) {
    return -1;
  }

  std::lock_guard scoped_lock(mutex_);
  dir_fd_to_entries_map_[fd] = std::move(entries);
  return fd;
}

int AssetManager::CloseDirectory(int fd) {
  {
    std::lock_guard scoped_lock(mutex_);
    auto search = dir_fd_to_entries_map_.find(fd);
    if (search == dir_fd_to_entries_map_.end()) {
      return -1;
    }
    dir_fd_to_entries_map_.erase(search);
  }  // Can't hold lock when calling close();
  return close(fd);
}

bool AssetManager::IsAssetDirFd(int fd) const {
  std::lock_guard scoped_lock(mutex_);
  return dir_fd_to_entries_map_.count(fd) == 1;
}

bool AssetManager::GetDirectoryEntries(
    int fd,
    std::vector<std::string>* entries) const {
  std::lock_guard scoped_lock(mutex_);
  auto search = dir_fd_to_entries_map_.find(fd);
  if (search == dir_fd_to_entries_map_.end()) {
    return false;
  }
  *entries = search->second;
  return true;
}

AssetManager::AssetManager() {
  const int kPathSize = PATH_MAX / 2;
  char path[kPathSize] = {0};
  SB_CHECK(SbSystemGetPath(kSbSystemPathTempDirectory, path, kPathSize))
      << "Unable to get system temp path for AssetManager.";
  SB_CHECK_LT(starboard::strlcat(path, "/asset_tmp", kPathSize), kPathSize)
      << "Unable to construct temp path for AssetManager.";
  tmp_root_ = path;
  ClearTempDir();
}

uint64_t AssetManager::AcquireInternalFd() {
  std::lock_guard scoped_lock(mutex_);
  do {
    ++internal_fd_;
  } while (in_use_internal_fd_set_.count(internal_fd_) == 1);
  in_use_internal_fd_set_.insert(internal_fd_);
  return internal_fd_;
}

std::string AssetManager::TempFilepath(uint64_t internal_fd) const {
  return tmp_root_ + "/" + std::to_string(internal_fd);
}

void AssetManager::ClearTempDir() {
  auto callback = [](const char* child, const struct stat*, int file_type,
                     struct FTW*) -> int { return remove(child); };
  nftw(tmp_root_.c_str(), callback, 32, FTW_DEPTH | FTW_PHYS);
  mkdir(tmp_root_.c_str(), 0700);
}

}  // namespace starboard

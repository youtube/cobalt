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

#ifndef STARBOARD_ANDROID_SHARED_ASSET_MANAGER_H_
#define STARBOARD_ANDROID_SHARED_ASSET_MANAGER_H_

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace starboard {

// This class handles opening/closing Android asset files as POSIX filehandles.
class AssetManager {
 public:
  static AssetManager* GetInstance();
  int Open(const char* path, int oflag);
  int Close(int fd);
  bool IsAssetFd(int fd) const;

  int OpenDirectory(const char* path);
  int CloseDirectory(int fd);
  bool IsAssetDirFd(int fd) const;
  bool GetDirectoryEntries(int fd, std::vector<std::string>* entries) const;

 private:
  AssetManager();
  ~AssetManager() { ClearTempDir(); }
  uint64_t AcquireInternalFd();
  std::string TempFilepath(uint64_t internal_fd) const;
  void ClearTempDir();

  std::string tmp_root_;
  mutable std::mutex mutex_;
  uint64_t internal_fd_ = 0;                       // Guarded by |mutex_|.
  std::set<uint64_t> in_use_internal_fd_set_;      // Guarded by |mutex_|.
  std::map<int, uint64_t> fd_to_internal_fd_map_;  // Guarded by |mutex_|.
  // Maps a reserved placeholder fd to the asset directory entries
  std::map<int, std::vector<std::string>> dir_fd_to_entries_map_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_ASSET_MANAGER_H_

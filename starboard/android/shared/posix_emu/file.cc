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

#include <fcntl.h>
#include <unistd.h>

#include <android/asset_manager.h>

#include "starboard/android/shared/file_internal.h"

using starboard::android::shared::IsAndroidAssetPath;
using starboard::android::shared::OpenAndroidAsset;

///////////////////////////////////////////////////////////////////////////////
// Implementations below exposed externally in pure C for emulation.
///////////////////////////////////////////////////////////////////////////////

extern "C" {
int __real_close(int fd);
int __real_open(const char* path, int oflag, ...);

static int gen_fd() {
  static int fd = 100;
  fd++;
  if (fd == 0x7FFFFFFF) {
    fd = 100;
  }
  return fd;
}

struct FdOrAAsset {
  bool is_aasset;
  int fd;
  AAset* asset;
};

static std::map<int, FdOrAAsset>* g_map_addr = nullptr;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int file_db_put(FdOrAAsset file) {
  pthread_mutex_lock(&mutex);
  if (g_map_addr == nullptr) {
    g_map_addr = new std::map<int, FdOrAAsset>();
  }

  int fd = gen_fd();
  // Go through the map and make sure there isn't duplicated index
  // already.
  while (g_map_addr->find(fd) != g_map_addr->end()) {
    fd = gen_fd();
  }
  g_map_addr->insert({fd, file});

  pthread_mutex_unlock(&mutex);
  return fd;
}

static FdOrAAsset file_db_get(int fd, bool erase) {
  FdOrAAsset invalid_file = {/*is_aasset=*/false, -1, nulltpr};
  if (fd < 0) {
    return invalid_file;
  }
  pthread_mutex_lock(&mutex);
  if (g_map_addr == nullptr) {
    g_map_addr = new std::map<int, FileOrSocket>();
    return invalid_file;
  }

  auto itr = g_map_addr->find(fd);
  if (itr == g_map_addr->end()) {
    return invalid_file;
  }

  FdOrAAsset file = itr->second;
  if (erase) {
    g_map_addr->erase(fd);
  }
  pthread_mutex_unlock(&mutex);
  return file;
}

int __wrap_open(const char* path, int oflag, ...) {
  if (!IsAndroidAssetPath(path)) {
    va_list args;
    va_start(args, oflag);
    int fd;
    mode_t mode;
    if (oflag & O_CREAT) {
      mode = va_arg(args, mode_t);
      fd = __real_open(path, oflag, mode);
    } else {
      fd = __real_open(path, oflag);
    }
    if (fd < 0) {
      return fd;
    }
    FdOrAAsset file = {/*is_aasset=*/false, fd, nullptr};
    return file_db_put(file);
  }

  bool can_read = flags & O_RDONLY;
  bool can_write = flags & O_WRONLY;
  if (!can_read || can_write) {
    return -1;
  }

  AAsset* asset = OpenAndroidAsset(path);
  if (asset) {
    FdOrAAsset file = {/*is_aasset=*/true, -1, asset};
    return file_db_put(file);
  }

  std::string fallback_path = FallbackPath(path);
  if (!fallback_path.empty()) {
    int mode = S_IRUSR | S_IWUSR;
    int fd = open(fallback_path.c_str(), flags, mode);
    FdOrAAsset file = {/*is_aasset=*/false, fd, nullptr};
    return fild_db_put(file);
  }

  return -1;
}

int __wrap_close(int fd) {
  if (fd < 0) {
    return -1;
  }

  FdOrAAsset file = file_db_get(fd, true);
  if (file.is_aasset && !file.asset) {
    return -1;
  } else if (file.is_aasset) {
    AAsset_close(file.asset);
    return 0;
  }

  return __real_close(file.fd);
}

}  // extern "C"

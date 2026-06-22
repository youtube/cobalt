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
#include <sys/uio.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "starboard/android/shared/asset_manager.h"
#include "starboard/android/shared/file_internal.h"
#include "starboard/common/log.h"
#include "starboard/configuration_constants.h"
#include "starboard/log.h"

using starboard::AssetManager;
using starboard::IsAndroidAssetPath;
using starboard::OpenAndroidAsset;

namespace {

bool OpenNeedsMode(int oflag) {
  // O_TMPFILE shares the O_DIRECTORY bit, so it must be masked exactly.
  return (oflag & O_CREAT) || (oflag & O_TMPFILE) == O_TMPFILE;
}

}  // namespace

// ///////////////////////////////////////////////////////////////////////////////
// // Implementations below exposed externally in pure C for emulation.
// ///////////////////////////////////////////////////////////////////////////////

extern "C" {

int __real_close(int fildes);
int __real_open(const char* path, int oflag, ...);
int __real_openat(int dirfd, const char* path, int oflag, ...);
ssize_t __real_writev(int fd, const struct iovec* iov, int iovcnt);

int __wrap_close(int fildes) {
  AssetManager* asset_manager = AssetManager::GetInstance();
  if (asset_manager->IsAssetFd(fildes)) {
    return asset_manager->Close(fildes);
  }
  return __real_close(fildes);
}

int __wrap_openat(int dirfd, const char* path, int oflag, ...) {
  if (!IsAndroidAssetPath(path)) {
    if (OpenNeedsMode(oflag)) {
      va_list args;
      va_start(args, oflag);
      mode_t mode = va_arg(args, int);
      va_end(args);
      return __real_openat(dirfd, path, oflag, mode);
    }
    return __real_openat(dirfd, path, oflag);
  }
  return AssetManager::GetInstance()->Open(path, oflag);
}

int __wrap_open(const char* path, int oflag, ...) {
  if (OpenNeedsMode(oflag)) {
    va_list args;
    va_start(args, oflag);
    mode_t mode = va_arg(args, int);
    va_end(args);
    return __wrap_openat(AT_FDCWD, path, oflag, mode);
  }
  return __wrap_openat(AT_FDCWD, path, oflag);
}

// Route stdout/stderr writes, which musl flushes via writev, through SbLogRaw
// so they reach Android logcat.
ssize_t __wrap_writev(int fd, const struct iovec* iov, int iovcnt) {
  if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
    std::string buffer;
    for (int i = 0; i < iovcnt; ++i) {
      if (iov[i].iov_base && iov[i].iov_len > 0) {
        buffer.append(static_cast<const char*>(iov[i].iov_base),
                      iov[i].iov_len);
      }
    }
    // SbLogRaw is line-oriented; split on newlines and drop the terminators.
    size_t start = 0, pos;
    while ((pos = buffer.find('\n', start)) != std::string::npos) {
      buffer[pos] = '\0';
      if (pos > start) {
        SbLogRaw(buffer.c_str() + start);
      }
      start = pos + 1;
    }
    if (start < buffer.size()) {
      SbLogRaw(buffer.c_str() + start);
    }
    return static_cast<ssize_t>(buffer.size());
  }
  return __real_writev(fd, iov, iovcnt);
}

}  // extern "C"

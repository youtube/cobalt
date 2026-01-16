// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/memfd_media_buffer_pool.h"

#include <android/api-level.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>

#include "starboard/common/check_op.h"
#include "starboard/common/experimental/media_buffer_pool.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

using ::starboard::common::experimental::IsPointerAnnotated;
using ::starboard::common::experimental::UnannotatePointer;

int CreateReliableCacheFd(const char* name, size_t size) {
  int fd = -1;
  int api_level = android_get_device_api_level();

  // 1. API 30+: memfd_create (Guaranteed)
  if (api_level >= 30) {
#ifndef __NR_memfd_create
#error __NR_memfd_create must be defined.
#endif
    fd = syscall(__NR_memfd_create, name, MFD_CLOEXEC);
    if (fd >= 0) {
      if (ftruncate(fd, size) >= 0) {
        SB_LOG(INFO) << "Created using memfd_create().";
        return fd;
      } else {
        SB_LOG(WARNING) << "Failed to truncate memfd to " << size << ".";
        close(fd);
        fd = -1;
      }
    } else {
      SB_LOG(WARNING) << "Failed to create memfd using memfd_create().";
    }
  }

  // 2. API 26-29: ASharedMemory (Ashmem-backed mostly)
  if (api_level >= 26) {
    void* lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
    if (lib) {
      typedef int (*ASharedMemory_create_func)(const char*, size_t);
      auto func = (ASharedMemory_create_func)dlsym(lib, "ASharedMemory_create");
      if (func) {
        fd = func(name, size);
        if (fd >= 0) {
          // Try `ftruncate()` to ensure it works.
          if (ftruncate(fd, size) >= 0) {
            SB_LOG(INFO) << "Created using ASharedMemory_create().";
            return fd;
          } else {
            SB_LOG(WARNING) << "Failed to truncate memfd to " << size << ".";
            close(fd);
            fd = -1;
          }
        } else {
          SB_LOG(WARNING) << "Failed to create memfd using "
                          << "ASharedMemory_create().";
        }
      }
      dlclose(lib);
    }
  }

  // 3. API < 26: Legacy /dev/ashmem based Ashmem, which we decided not to
  //    support for now.
  return fd;
}

}  // namespace

// static
MemFdMediaBufferPool* MemFdMediaBufferPool::Get() {
  static MemFdMediaBufferPool instance;

  if (instance.fd_ < 0) {
    SB_LOG(WARNING) << "Failed to create memfd for MediaBufferPool.";
    return nullptr;
  }

  return &instance;
}

MemFdMediaBufferPool::MemFdMediaBufferPool() {
  fd_ = CreateReliableCacheFd("cobalt_media_buffer_pool", 0);

  if (fd_ >= 0) {
    SB_LOG(INFO) << "Created memfd for MediaBufferPool.";
  } else {
    SB_LOG(WARNING) << "Failed to create memfd in constructor.";
  }
}

MemFdMediaBufferPool::~MemFdMediaBufferPool() {
  if (fd_ >= 0) {
    close(fd_);
  }
}

void MemFdMediaBufferPool::ShrinkToZero() {
  // fd_ is guaranteed to be >= 0 if Get() returned this instance.
  SB_DCHECK_GE(fd_, 0);

  // Truncate the file to zero bytes to effectively reset the pool and
  // signal to the OS that the physical pages can be reclaimed.
  auto start = CurrentMonotonicTime();
  if (ftruncate(fd_, 0) == 0) {
    SB_LOG(INFO) << "Shrunk memfd to 0 takes " << CurrentMonotonicTime() - start
                 << " microseconds.";
  } else {
    SB_LOG(ERROR) << "Failed to shrink memfd to 0.";
  }
  current_capacity_ = 0;
}

bool MemFdMediaBufferPool::ExpandTo(size_t size) {
  // fd_ is guaranteed to be >= 0 if Get() returned this instance.
  SB_DCHECK_GE(fd_, 0);

  if (size <= current_capacity_) {
    return true;
  }

  auto start = CurrentMonotonicTime();
  if (ftruncate(fd_, size) != 0) {
    SB_LOG(ERROR) << "Failed to expand memfd to " << size;
    return false;
  }

  SB_LOG(INFO) << "Expanded memfd to " << size << " takes "
               << CurrentMonotonicTime() - start << " microseconds.";
  current_capacity_ = size;
  return true;
}

void MemFdMediaBufferPool::Write(intptr_t position,
                                 const void* data,
                                 size_t size) {
  // fd_ is guaranteed to be >= 0 if Get() returned this instance.
  SB_DCHECK_GE(fd_, 0);
  SB_DCHECK(!IsPointerAnnotated(position));

  if (static_cast<size_t>(position) + size > current_capacity_) {
    SB_LOG(ERROR) << "MemFdMediaBufferPool: Write out of bounds. pos="
                  << position << " size=" << size
                  << " capacity=" << current_capacity_;
    return;
  }

  size_t total_written = 0;
  while (total_written < size) {
    ssize_t bytes_written =
        pwrite(fd_, static_cast<const uint8_t*>(data) + total_written,
               size - total_written, position + total_written);
    if (bytes_written < 0) {
      if (errno == EINTR) {
        continue;
      }
      SB_LOG(ERROR) << "MemFdMediaBufferPool: Failed to write data. res="
                    << bytes_written << " errno=" << errno;
      break;
    }
    if (bytes_written == 0) {
      SB_LOG(ERROR) << "MemFdMediaBufferPool: pwrite returned 0. total_written="
                    << total_written << " size=" << size;
      break;
    }
    total_written += bytes_written;
  }

#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  SB_CHECK_EQ(total_written, size);
#endif  // BUILDFLAG(COBALT_IS_RELEASE_BUILD)
}

void MemFdMediaBufferPool::Read(intptr_t position, void* buffer, size_t size) {
  // fd_ is guaranteed to be >= 0 if Get() returned this instance.
  SB_DCHECK_GE(fd_, 0);
  SB_DCHECK(!IsPointerAnnotated(position));

  if (static_cast<size_t>(position) + size > current_capacity_) {
    SB_LOG(ERROR) << "MemFdMediaBufferPool: Read out of bounds. pos="
                  << position << " size=" << size
                  << " capacity=" << current_capacity_;
    return;
  }

  size_t total_read = 0;
  while (total_read < size) {
    ssize_t bytes_read = pread(fd_, static_cast<uint8_t*>(buffer) + total_read,
                               size - total_read, position + total_read);
    if (bytes_read < 0) {
      if (errno == EINTR) {
        continue;
      }
      SB_LOG(ERROR) << "MemFdMediaBufferPool: Failed to read data. res="
                    << bytes_read << " errno=" << errno;
      break;
    }
    if (bytes_read == 0) {
      SB_LOG(ERROR) << "MemFdMediaBufferPool: pread returned 0. total_read="
                    << total_read << " size=" << size;
      break;
    }
    total_read += bytes_read;
  }

#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  SB_CHECK_EQ(total_read, size);
#endif  // BUILDFLAG(COBALT_IS_RELEASE_BUILD)
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

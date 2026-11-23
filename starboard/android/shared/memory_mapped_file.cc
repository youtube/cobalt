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

#include "starboard/android/shared/memory_mapped_file.h"

#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "starboard/android/shared/file_internal.h"
#include "starboard/common/log.h"
#include "starboard/elf_loader/file_open.h"
#include "starboard/extension/memory_mapped_file.h"
#include "starboard/shared/posix/page_internal.h"

namespace starboard {

namespace {

int ToMmapProtect(SbMemoryMapFlags flags) {
  int protect = PROT_NONE;
  if (flags & kSbMemoryMapProtectRead) {
    protect |= PROT_READ;
  }
  if (flags & kSbMemoryMapProtectWrite) {
    protect |= PROT_WRITE;
  }
  if (flags & kSbMemoryMapProtectExec) {
    protect |= PROT_EXEC;
  }
  return protect;
}

// Maps |size| bytes at |file_offset| of the asset at |path| from the APK
// itself. Opening an asset with open() copies it to the cache directory, and
// apps can't map app data files as executable, so the copy can't hold code.
// Only assets stored uncompressed at a page aligned offset can be mapped.
void* MapAsset(void* address,
               const char* path,
               SbMemoryMapFlags flags,
               int64_t file_offset,
               int64_t size) {
  int64_t start = 0;
  int64_t length = 0;
  const int fd = elf_loader::OpenFileInPlace(path, &start, &length);
  if (fd < 0) {
    SB_LOG(ERROR) << "Can't map " << path << ", it must be stored uncompressed";
    return nullptr;
  }

  void* result = nullptr;
  const int64_t apk_offset = start + file_offset;
  if (file_offset < 0 || size <= 0 || file_offset > length - size) {
    SB_LOG(ERROR) << "Range is outside " << path;
  } else if (apk_offset % getpagesize() != 0) {
    SB_LOG(ERROR) << "Can't map " << path
                  << ", it isn't page aligned in the APK";
  } else {
    void* mapped =
        mmap(address, size, ToMmapProtect(flags),
             MAP_PRIVATE | (address ? MAP_FIXED : 0), fd, apk_offset);
    if (mapped == MAP_FAILED) {
      SB_LOG(ERROR) << "Failed to map " << path << ": " << strerror(errno);
    } else {
      result = mapped;
    }
  }
  close(fd);
  return result;
}

void* MemoryMapFile(void* address,
                    const char* path,
                    SbMemoryMapFlags flags,
                    int64_t file_offset,
                    int64_t size) {
  if (IsAndroidAssetPath(path)) {
    return MapAsset(address, path, flags, file_offset, size);
  }
  return SbPageMapFile(address, path, flags, file_offset, size);
}

const CobaltExtensionMemoryMappedFileApi kMemoryMappedFileApi = {
    kCobaltExtensionMemoryMappedFileName,
    1,
    &MemoryMapFile,
};

}  // namespace

const void* GetMemoryMappedFileApi() {
  return &kMemoryMappedFileApi;
}

}  // namespace starboard

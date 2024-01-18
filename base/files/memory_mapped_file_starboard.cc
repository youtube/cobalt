// Copyright 2023 Google Inc. All Rights Reserved.
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

#include "base/files/memory_mapped_file.h"

#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/build_config.h"
#include "starboard/extension/memory_mapped_file.h"
#include "starboard/memory.h"

namespace base {

MemoryMappedFile::MemoryMappedFile() = default;

bool MemoryMappedFile::MapFileRegionToMemory(
    const MemoryMappedFile::Region& region,
    Access access) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  off_t map_start = 0;
  size_t map_size = 0;
  int32_t data_offset = 0;

  if (region == MemoryMappedFile::Region::kWholeFile) {
    int64_t file_len = file_.GetLength();
    if (file_len < 0) {
      DPLOG(ERROR) << "fstat " << file_.GetPlatformFile();
      return false;
    }
    if (!IsValueInRangeForNumericType<size_t>(file_len))
      return false;
    map_size = static_cast<size_t>(file_len);
    length_ = map_size;
  } else {
    // The region can be arbitrarily aligned. mmap, instead, requires both the
    // start and size to be page-aligned. Hence, we map here the page-aligned
    // outer region [|aligned_start|, |aligned_start| + |size|] which contains
    // |region| and then add up the |data_offset| displacement.
    int64_t aligned_start = 0;
    size_t aligned_size = 0;
    CalculateVMAlignedBoundaries(region.offset, region.size, &aligned_start,
                                 &aligned_size, &data_offset);

    // Ensure that the casts in the mmap call below are sane.
    if (aligned_start < 0 ||
        !IsValueInRangeForNumericType<off_t>(aligned_start)) {
      DLOG(ERROR) << "Region bounds are not valid for mmap";
      return false;
    }

    map_start = static_cast<off_t>(aligned_start);
    map_size = aligned_size;
    length_ = region.size;
  }

  SbMemoryMapFlags flags = kSbMemoryMapProtectRead;
  switch (access) {
    case READ_ONLY:
      flags = kSbMemoryMapProtectRead;
      break;

    case READ_WRITE:
      flags = kSbMemoryMapProtectReadWrite;
      break;

    case READ_WRITE_EXTEND:
      flags = kSbMemoryMapProtectReadWrite;

      break;
  }
  const auto* memory_mapped_file_extension =
      reinterpret_cast<const CobaltExtensionMemoryMappedFileApi*>(
          SbSystemGetExtension(kCobaltExtensionMemoryMappedFileName));
  if (memory_mapped_file_extension &&
      strcmp(memory_mapped_file_extension->name,
             kCobaltExtensionMemoryMappedFileName) == 0 &&
      memory_mapped_file_extension->version >= 1) {
    data_ = static_cast<uint8_t*>(memory_mapped_file_extension->MemoryMapFile(
        nullptr, file_.GetFileName().c_str(), flags,
        map_start, map_size));
  }
  if (data_ == MAP_FAILED) {
    DPLOG(ERROR) << "mmap " << file_.GetFileName().c_str();
    return false;
  }

  data_ += data_offset;
  return true;
}

void MemoryMappedFile::CloseHandles() {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  if (data_ != nullptr) {
    munmap(data_.ExtractAsDangling(), length_);
  }
  file_.Close();
  length_ = 0;
}

}  // namespace base

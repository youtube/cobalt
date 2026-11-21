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

#ifndef MEDIA_STARBOARD_MEDIA_POOL_DIRECT_MAP_H_
#define MEDIA_STARBOARD_MEDIA_POOL_DIRECT_MAP_H_

#include <stddef.h>

#include <optional>

#include "media/base/media_export.h"

namespace media {

// Page-aligned mmap() blocks for the DecoderBufferAllocator pool
// (kCobaltMediaPoolSoftFail).
//
// Without it, each pool block is a posix_memalign() of up to 4 MiB. On 32-bit
// builds, PartitionAlloc serves that as a direct map that must start on a
// 2 MiB boundary, so a 4 MiB block can need a 6 MiB hole, and a failure is
// fatal. Blocks from Map() only need a hole of their own size plus two guard
// pages, and a failure returns nullptr so the pool can retry smaller or report
// the failure.
class MEDIA_EXPORT MediaPoolDirectMap {
 public:
  MediaPoolDirectMap() = delete;

  // The most live blocks Map() tracks; Map() fails beyond that.
  static constexpr size_t kMaxBlocks = 1024;

  // True when pool blocks should come from Map() and fail softly: the
  // kCobaltMediaPoolSoftFail feature is enabled and this is a 32-bit build
  // (always false on 64-bit builds), or a test forced it.
  static bool IsEnabled();

  // Maps `*size` bytes rounded up to whole pages, read/write, with a PROT_NONE
  // guard page below and above. On success, writes the rounded size back to
  // `*size` and returns the start of the usable range. Returns nullptr (and
  // leaves `*size` alone) if `*size` is 0, the mapping fails or kMaxBlocks
  // blocks are live.
  static void* Map(size_t* size);

  // Unmaps a block returned by Map(), guard pages included. Returns false if
  // `memory` is not the start of a live Map() block.
  static bool Unmap(void* memory);

  // Testing: overrides IsEnabled() (std::nullopt restores the feature check).
  static void SetEnabledForTesting(std::optional<bool> enabled);
  // Testing: makes every Map() fail.
  static void SetMapFailForTesting(bool fail);
  // Testing: makes Map() fail for sizes above `max_bytes` (std::nullopt: no
  // limit).
  static void SetMapFailAboveForTesting(std::optional<size_t> max_bytes);
  // Testing: the number of live Map() blocks.
  static size_t BlockCountForTesting();
};

}  // namespace media

#endif  // MEDIA_STARBOARD_MEDIA_POOL_DIRECT_MAP_H_

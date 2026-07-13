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

#include "starboard/tools/zstd_compress/zstd_compress.h"

#include <iostream>
#include <vector>

#include "third_party/zstd/src/lib/zstd.h"

namespace starboard {
namespace tools {
namespace zstd_compress {

bool Compress(const std::vector<char>& src_buffer,
              std::vector<char>& dst_buffer) {
  if (src_buffer.empty()) {
    std::cerr << "Source buffer is empty\n";
    return false;
  }

  ZSTD_CCtx* cctx = ZSTD_createCCtx();
  if (!cctx) {
    std::cerr << "Failed to create ZSTD_CCtx\n";
    return false;
  }

  dst_buffer.clear();
  const size_t total_size = src_buffer.size();

  for (int i = 0; i < kNumFrames; ++i) {
    size_t chunk_start =
        (total_size * static_cast<size_t>(i)) / static_cast<size_t>(kNumFrames);
    size_t chunk_end = (total_size * static_cast<size_t>(i + 1)) /
                       static_cast<size_t>(kNumFrames);
    size_t chunk_size = chunk_end - chunk_start;

    if (chunk_size == 0) {
      continue;
    }

    size_t bound = ZSTD_compressBound(chunk_size);
    std::vector<char> frame_buf(bound);

    size_t encoded_size = ZSTD_compressCCtx(cctx, frame_buf.data(), bound,
                                            src_buffer.data() + chunk_start,
                                            chunk_size, kCompressionLevel);

    if (ZSTD_isError(encoded_size)) {
      std::cerr << "ZSTD_compressCCtx failed: "
                << ZSTD_getErrorName(encoded_size) << "\n";
      ZSTD_freeCCtx(cctx);
      return false;
    }

    dst_buffer.insert(dst_buffer.end(), frame_buf.data(),
                      frame_buf.data() + encoded_size);
  }

  ZSTD_freeCCtx(cctx);
  return !dst_buffer.empty();
}

}  // namespace zstd_compress
}  // namespace tools
}  // namespace starboard

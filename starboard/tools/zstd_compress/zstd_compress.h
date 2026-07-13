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

#ifndef STARBOARD_TOOLS_ZSTD_COMPRESS_ZSTD_COMPRESS_H_
#define STARBOARD_TOOLS_ZSTD_COMPRESS_ZSTD_COMPRESS_H_

#include <vector>

namespace starboard {
namespace tools {
namespace zstd_compress {

constexpr int kNumFrames = 64;
constexpr int kCompressionLevel = 3;

// Compresses src_buffer into dst_buffer across kNumFrames independent
// Zstandard frames, of ~evenly divided uncompressed source size, using
// kCompressionLevel.
bool Compress(const std::vector<char>& src_buffer,
              std::vector<char>& dst_buffer);

}  // namespace zstd_compress
}  // namespace tools
}  // namespace starboard

#endif  // STARBOARD_TOOLS_ZSTD_COMPRESS_ZSTD_COMPRESS_H_

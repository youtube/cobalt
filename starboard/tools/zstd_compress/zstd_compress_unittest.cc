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

#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zstd/src/lib/zstd.h"

namespace starboard {
namespace zstd_compress {
namespace {

std::vector<char> CreateTestData(size_t size) {
  std::vector<char> data;
  data.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    data.push_back(static_cast<char>('A' + (i % 26)));
  }
  return data;
}

TEST(ZstdCompressTest, EmptyInputFails) {
  std::vector<char> src;
  std::vector<char> dst;
  EXPECT_FALSE(Compress(src, dst));
}

TEST(ZstdCompressTest, CompressesIntoCorrectNumberOfFrames) {
  // Use a size proportional to kNumFrames (>= kNumFrames) to guarantee every
  // frame receives at least 1 byte and all kNumFrames frames are constructed.
  const size_t test_data_size = kNumFrames * 1000;
  std::vector<char> src = CreateTestData(test_data_size);
  std::vector<char> dst;

  ASSERT_TRUE(Compress(src, dst));
  EXPECT_FALSE(dst.empty());

  size_t offset = 0;
  int frame_count = 0;
  size_t total_decompressed_size = 0;

  while (offset < dst.size()) {
    size_t remaining = dst.size() - offset;
    size_t compressed_frame_size =
        ZSTD_findFrameCompressedSize(dst.data() + offset, remaining);
    ASSERT_FALSE(ZSTD_isError(compressed_frame_size));

    unsigned long long decompressed_frame_size =
        ZSTD_getFrameContentSize(dst.data() + offset, compressed_frame_size);
    ASSERT_NE(decompressed_frame_size, ZSTD_CONTENTSIZE_ERROR);
    ASSERT_NE(decompressed_frame_size, ZSTD_CONTENTSIZE_UNKNOWN);

    total_decompressed_size += static_cast<size_t>(decompressed_frame_size);
    offset += compressed_frame_size;
    frame_count++;
  }

  EXPECT_EQ(frame_count, kNumFrames);
  EXPECT_EQ(total_decompressed_size, src.size());
}

TEST(ZstdCompressTest, DecompressesToOriginalData) {
  const size_t test_data_size = kNumFrames * 1000;
  std::vector<char> src = CreateTestData(test_data_size);
  std::vector<char> dst;

  ASSERT_TRUE(Compress(src, dst));

  std::vector<char> decompressed(src.size());
  size_t result = ZSTD_decompress(decompressed.data(), decompressed.size(),
                                  dst.data(), dst.size());
  ASSERT_FALSE(ZSTD_isError(result));
  EXPECT_EQ(result, src.size());
  EXPECT_EQ(decompressed, src);
}

}  // namespace
}  // namespace zstd_compress
}  // namespace starboard

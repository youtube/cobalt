// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/decoder_buffer_allocator.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "media/base/demuxer_stream.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO: b/369245553 - Cobalt: As of today the tests here are used to experiment
//                     different allocation patterns.  Consider setting a real
//                     limit and fail allocation patterns taking excessive
//                     memory.

namespace media {
namespace {

std::vector<std::string> ReadAllocationLogFile(const std::string& name) {
  base::FilePath file_path = GetTestDataFilePath(name);

  int64_t file_size = 0;
  CHECK(base::GetFileSize(file_path, &file_size))
      << "Failed to get file size for '" << name << "'";
  CHECK_GE(file_size, 0);

  std::string buffer;

  buffer.resize(static_cast<size_t>(file_size));
  CHECK_EQ(file_size, base::ReadFile(file_path, buffer.data(), file_size))
      << "Failed to read '" << name << "'";

  return base::SplitStringUsingSubstr(buffer, "\n", base::TRIM_WHITESPACE,
                                      base::SPLIT_WANT_NONEMPTY);
}

int StringToInt(base::StringPiece input) {
  int result;

  CHECK(base::StringToInt(input, &result));

  return result;
}

TEST(DecoderBufferAllocator, CapacityUnderLimit) {
  DecoderBufferAllocator allocator(DecoderBufferAllocator::Type::kLocal);
  auto allocations =
      ReadAllocationLogFile("starboard/allocations_1La4QzGeaaQ.txt");

  std::map<std::string, void*> pointer_to_pointer_map;
  size_t max_allocated = 0, max_capacity = 0;

  for (auto&& allocation : allocations) {
    auto tokens = base::SplitStringUsingSubstr(
        allocation, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (tokens.size() == 5) {
      // In the format of "allocate <pointer> <type> <size> <alignment>"
      CHECK_EQ(tokens[0], "allocate");
      CHECK(pointer_to_pointer_map.find(tokens[1]) ==
            pointer_to_pointer_map.end());
      // Type has to be 0 (UNKNOWN), 1 (AUDIO), or 2 (VIDEO).
      CHECK(tokens[2] == "0" || tokens[2] == "1" || tokens[2] == "2");

      DemuxerStream::Type type =
          static_cast<DemuxerStream::Type>(StringToInt(tokens[2]));
      int size = StringToInt(tokens[3]);
      int alignment = StringToInt(tokens[4]);

      void* p = allocator.Allocate(type, size, alignment);

      pointer_to_pointer_map[tokens[1]] = p;
      max_allocated = std::max(max_allocated, allocator.GetAllocatedMemory());
      max_capacity =
          std::max(max_capacity, allocator.GetCurrentMemoryCapacity());
    } else {
      // In the format of "free <pointer> <type> <size>"
      CHECK_EQ(tokens.size(), 4);
      CHECK_EQ(tokens[0], "free");
      CHECK(pointer_to_pointer_map.find(tokens[1]) !=
            pointer_to_pointer_map.end());

      int size = StringToInt(tokens[3]);

      allocator.Free(pointer_to_pointer_map[tokens[1]], size);

      CHECK_EQ(pointer_to_pointer_map.erase(tokens[1]), 1);
    }
  }

  LOG(INFO) << "DecoderBufferAllocator reached max allocated of "
            << max_allocated << ", and max capacity of " << max_capacity;
}

TEST(DecoderBufferAllocator, CapacityByType) {
  constexpr int kInitialCapacity = 0;
  constexpr int kAllocationIncrementOther = 1024 * 1024;
  constexpr int kAllocationIncrementVideo = 4 * 1024 * 1024;

  DecoderBufferAllocator allocators[] = {
      // 0 => UNKNOWN (Used by MojoRenderer)
      DecoderBufferAllocator(DecoderBufferAllocator::Type::kLocal, false,
                             kInitialCapacity, kAllocationIncrementOther),
      // 1 => AUDIO
      DecoderBufferAllocator(DecoderBufferAllocator::Type::kLocal, false,
                             kInitialCapacity, kAllocationIncrementOther),
      // 2 => VIDEO
      DecoderBufferAllocator(DecoderBufferAllocator::Type::kLocal, false,
                             kInitialCapacity, kAllocationIncrementVideo),
  };
  size_t max_allocated[3] = {};
  size_t max_capacity[3] = {};

  auto allocations =
      ReadAllocationLogFile("starboard/allocations_1La4QzGeaaQ.txt");

  std::map<std::string, void*> pointer_to_pointer_map;

  for (auto&& allocation : allocations) {
    auto tokens = base::SplitStringUsingSubstr(
        allocation, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    if (tokens.size() == 5) {
      // In the format of "allocate <pointer> <type> <size> <alignment>"
      CHECK_EQ(tokens[0], "allocate");
      CHECK(pointer_to_pointer_map.find(tokens[1]) ==
            pointer_to_pointer_map.end());
      // Type has to be 0 (UNKNOWN), 1 (AUDIO), or 2 (VIDEO).
      CHECK(tokens[2] == "0" || tokens[2] == "1" || tokens[2] == "2");

      DemuxerStream::Type type =
          static_cast<DemuxerStream::Type>(StringToInt(tokens[2]));
      int size = StringToInt(tokens[3]);
      int alignment = StringToInt(tokens[4]);

      pointer_to_pointer_map[tokens[1]] =
          allocators[type].Allocate(type, size, alignment);
      max_allocated[type] =
          std::max(max_allocated[type], allocators[type].GetAllocatedMemory());
      max_capacity[type] = std::max(
          max_capacity[type], allocators[type].GetCurrentMemoryCapacity());
    } else {
      // In the format of "free <pointer>"
      CHECK_EQ(tokens.size(), 4);
      CHECK_EQ(tokens[0], "free");
      CHECK(pointer_to_pointer_map.find(tokens[1]) !=
            pointer_to_pointer_map.end());
      // Type has to be 0 (UNKNOWN), 1 (AUDIO), or 2 (VIDEO).
      CHECK(tokens[2] == "0" || tokens[2] == "1" || tokens[2] == "2");

      DemuxerStream::Type type =
          static_cast<DemuxerStream::Type>(StringToInt(tokens[2]));
      int size = StringToInt(tokens[3]);

      allocators[type].Free(pointer_to_pointer_map[tokens[1]], size);
      CHECK_EQ(pointer_to_pointer_map.erase(tokens[1]), 1);
    }
  }

  for (int i = 0; i < 3; ++i) {
    LOG(INFO) << "DecoderBufferAllocator (" << i
              << ") reached max allocated of " << max_allocated[i]
              << ", and max capacity of " << max_capacity[i];
  }
}

}  // namespace
}  // namespace media

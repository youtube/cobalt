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
#include <numeric>
#include <string>
#include <unordered_map>
#include <unordered_set>
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

using ::testing::Values;

std::vector<std::string> ReadAllocationLogFile(const std::string& name) {
  LOG(INFO) << "Loading " << name << " ...";

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

class DecoderBufferAllocatorTest
    : public ::testing::TestWithParam<std::string> {
 protected:
  struct Operation {
    enum class Type { kAllocate, kFree } operation_type;
    std::string pointer;
    DemuxerStream::Type buffer_type;
    int size;
    int alignment = 0;  // Only used when `operation_type` is `kAllocate`.
  };

  void SetUp() {
    auto allocations = ReadAllocationLogFile(GetParam());
    std::unordered_set<std::string> pointers;

    for (auto&& allocation : allocations) {
      auto tokens = base::SplitStringUsingSubstr(
          allocation, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      CHECK_GE(tokens.size(), 4);

      const std::string& pointer = tokens[1];
      const DemuxerStream::Type buffer_type =
          static_cast<DemuxerStream::Type>(StringToInt(tokens[2]));
      const int size = StringToInt(tokens[3]);

      CHECK(buffer_type == DemuxerStream::UNKNOWN ||
            buffer_type == DemuxerStream::AUDIO ||
            buffer_type == DemuxerStream::VIDEO);

      if (tokens.size() == 5) {
        // In the format of "allocate <pointer> <buffer_type> <size>
        // <alignment>"
        CHECK_EQ(tokens[0], "allocate");
        CHECK_EQ(pointers.count(pointer), 0);

        int alignment = StringToInt(tokens[4]);

        pointers.insert(pointer);

        operations_.emplace_back(Operation{Operation::Type::kAllocate, pointer,
                                           buffer_type, size, alignment});
      } else {
        // In the format of "free <pointer> <buffer_type> <size>"
        CHECK_EQ(tokens.size(), 4);
        CHECK_EQ(tokens[0], "free");
        CHECK_EQ(pointers.erase(pointer), 1);

        operations_.emplace_back(
            Operation{Operation::Type::kFree, pointer, buffer_type, size});
      }
    }
  }

  std::vector<Operation> operations_;
};

TEST_P(DecoderBufferAllocatorTest, CapacityUnderLimit) {
  DecoderBufferAllocator allocator(DecoderBufferAllocator::Type::kLocal);
  std::unordered_map<std::string, void*> pointer_to_pointer_map;
  size_t max_allocated = 0, max_capacity = 0;

  for (auto&& operation : operations_) {
    if (operation.operation_type == Operation::Type::kAllocate) {
      CHECK_EQ(pointer_to_pointer_map.count(operation.pointer), 0);

      void* p = allocator.Allocate(operation.buffer_type, operation.size,
                                   operation.alignment);

      pointer_to_pointer_map[operation.pointer] = p;
      max_allocated = std::max(max_allocated, allocator.GetAllocatedMemory());
      max_capacity =
          std::max(max_capacity, allocator.GetCurrentMemoryCapacity());
    } else {
      CHECK_EQ(operation.operation_type, Operation::Type::kFree);

      allocator.Free(pointer_to_pointer_map[operation.pointer], operation.size);

      CHECK_EQ(pointer_to_pointer_map.erase(operation.pointer), 1);
    }
  }

  LOG(INFO) << "DecoderBufferAllocator reached max allocated of "
            << max_allocated << ", and max capacity of " << max_capacity;
}

TEST_P(DecoderBufferAllocatorTest, CapacityByType) {
  struct AllocatorConfig {
    int initial_capacity[3];      // UNKNOWN, AUDIO, VIDEO
    int allocation_increment[3];  // UNKNOWN, AUDIO, VIDEO
  };

  constexpr AllocatorConfig kConfigs[] = {
      {{0, 0, 0}, {4 * 1024 * 1024, 4 * 1024 * 1024, 4 * 1024 * 1024}},
      {{0, 5 * 1024 * 1024, 30 * 1024 * 1024},
       {4 * 1024 * 1024, 1 * 1024 * 1024, 4 * 1024 * 1024}},
  };

  for (int i = 0; i < sizeof(kConfigs) / sizeof(*kConfigs); ++i) {
    size_t max_total_allocated = 0;
    size_t max_total_capacity = 0;

    for (auto buffer_type :
         {DemuxerStream::UNKNOWN, DemuxerStream::AUDIO, DemuxerStream::VIDEO}) {
      LOG(INFO) << "Checking allocation for initial capacity "
                << kConfigs[i].initial_capacity[buffer_type]
                << " and allocation increment "
                << kConfigs[i].allocation_increment[buffer_type];

      DecoderBufferAllocator allocator(
          DecoderBufferAllocator::Type::kLocal, false,
          kConfigs[i].initial_capacity[buffer_type],
          kConfigs[i].allocation_increment[buffer_type]);
      std::unordered_map<std::string, void*> pointer_to_pointer_map;
      size_t max_allocated = 0;
      size_t max_capacity = 0;

      for (auto&& operation : operations_) {
        if (operation.buffer_type != buffer_type) {
          continue;
        }

        if (operation.operation_type == Operation::Type::kAllocate) {
          CHECK_EQ(pointer_to_pointer_map.count(operation.pointer), 0);

          pointer_to_pointer_map[operation.pointer] = allocator.Allocate(
              operation.buffer_type, operation.size, operation.alignment);
          max_allocated =
              std::max(max_allocated, allocator.GetAllocatedMemory());
          max_capacity =
              std::max(max_capacity, allocator.GetCurrentMemoryCapacity());
        } else {
          CHECK_EQ(operation.operation_type, Operation::Type::kFree);

          allocator.Free(pointer_to_pointer_map[operation.pointer],
                         operation.size);
          CHECK_EQ(pointer_to_pointer_map.erase(operation.pointer), 1);
        }
      }

      LOG(INFO) << "Max allocated for type " << buffer_type << " is "
                << max_allocated << ", and max capacity is " << max_capacity;
      max_total_allocated += max_allocated;
      max_total_capacity += max_capacity;
    }

    LOG(INFO) << "Total max allocated: " << max_total_allocated
              << ", and total max capacity: " << max_total_capacity;
  }
}

INSTANTIATE_TEST_CASE_P(DecoderBufferAllocatorTests,
                        DecoderBufferAllocatorTest,
                        Values("starboard/allocations_1La4QzGeaaQ.txt",
                               "starboard/allocations_8k.txt",
                               "starboard/allocations_PMwaIrjiz8w.txt"),
                        [](const ::testing::TestParamInfo<std::string>& info) {
                          std::string name = info.param;
                          std::replace(name.begin(), name.end(), '.', '_');
                          std::replace(name.begin(), name.end(), '/', '_');
                          return name;
                        });

}  // namespace
}  // namespace media

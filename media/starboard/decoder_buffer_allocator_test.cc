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
#include "base/strings/string_split.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO: b/369245553 - Cobalt: As of today the tests here are used to experiment
//                     different allocation patterns.  Consider setting a real
//                     limit and fail allocation patterns taking excessive
//                     memory.

namespace media {
namespace {

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;

struct Operation {
  enum class Type { kAllocate, kFree } operation_type;
  std::string pointer;
  DemuxerStream::Type buffer_type;
  int size;
  int alignment = 0;  // Only used when `operation_type` is `kAllocate`.
};

int StringToInt(std::string_view input) {
  int result;

  CHECK(base::StringToInt(input, &result));

  return result;
}

const std::vector<Operation>& ReadAllocationLogFile(const std::string& name) {
  static std::unordered_map<std::string, std::vector<Operation>>
      name_to_operations_map;
  auto iter = name_to_operations_map.find(name);

  if (iter == name_to_operations_map.end()) {
    LOG(INFO) << "Loading " << name << " ...";

    base::FilePath file_path = GetTestDataFilePath(name);

    std::optional<int64_t> file_size = base::GetFileSize(file_path);
    CHECK(file_size.has_value())
        << "Failed to get file size for '" << name << "'";
    CHECK_GE(file_size.value(), 0);

    std::string buffer;

    buffer.resize(static_cast<size_t>(file_size.value()));
    CHECK_EQ(file_size.value(),
             base::ReadFile(file_path, buffer.data(), file_size.value()))
        << "Failed to read '" << name << "'";

    auto allocations = base::SplitStringUsingSubstr(
        buffer, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    std::vector<Operation> operations;
    std::unordered_set<std::string> pointers;

    for (auto&& allocation : allocations) {
      auto tokens = base::SplitStringUsingSubstr(
          allocation, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      CHECK_GE(tokens.size(), 4u);

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
        CHECK_EQ(pointers.count(pointer), 0u);

        int alignment = StringToInt(tokens[4]);

        pointers.insert(pointer);

        operations.emplace_back(Operation{Operation::Type::kAllocate, pointer,
                                          buffer_type, size, alignment});
      } else {
        // In the format of "free <pointer> <buffer_type> <size>"
        CHECK_EQ(tokens.size(), 4u);
        CHECK_EQ(tokens[0], "free");
        CHECK_EQ(pointers.erase(pointer), 1u);

        operations.emplace_back(
            Operation{Operation::Type::kFree, pointer, buffer_type, size});
      }
    }

    iter =
        name_to_operations_map.insert(std::make_pair(name, operations)).first;
  }

  return iter->second;
}

class DecoderBufferAllocatorTest
    : public ::testing::TestWithParam<std::tuple<std::string, bool>> {
 protected:
  void SetUp() {
    scoped_list.InitWithFeatureState(
        kCobaltDecoderBufferAllocatorWithInPlaceMetadata,
        std::get<1>(GetParam()));
  }

  base::test::ScopedFeatureList scoped_list;
};

TEST_P(DecoderBufferAllocatorTest, VerifyAndCacheAllocationLogs) {
  ReadAllocationLogFile(std::get<0>(GetParam()));
}

TEST_P(DecoderBufferAllocatorTest, CapacityUnderLimit) {
  DecoderBufferAllocator allocator(DecoderBufferAllocator::Type::kLocal);
  std::unordered_map<std::string, void*> pointer_to_pointer_map;
  size_t max_allocated = 0, max_capacity = 0;
  base::Time start_time = base::Time::Now();
  base::Time last_allocate_time = start_time;
  int free_operations_since_last_allocate = 0;

  const auto& operations = ReadAllocationLogFile(std::get<0>(GetParam()));

  for (auto&& operation : operations) {
    if (operation.operation_type == Operation::Type::kAllocate) {
      CHECK_EQ(pointer_to_pointer_map.count(operation.pointer), 0u);

      void* p = allocator.Allocate(operation.buffer_type, operation.size,
                                   operation.alignment);

      pointer_to_pointer_map[operation.pointer] = p;
      max_allocated = std::max(max_allocated, allocator.GetAllocatedMemory());
      max_capacity =
          std::max(max_capacity, allocator.GetCurrentMemoryCapacity());
      last_allocate_time = base::Time::Now();
      free_operations_since_last_allocate = 0;
    } else {
      CHECK_EQ(operation.operation_type, Operation::Type::kFree);

      allocator.Free(pointer_to_pointer_map[operation.pointer], operation.size);
      ++free_operations_since_last_allocate;

      CHECK_EQ(pointer_to_pointer_map.erase(operation.pointer), 1u);
    }
  }

  LOG(INFO) << "DecoderBufferAllocator reached max allocated of "
            << max_allocated << ", and max capacity of " << max_capacity;
  LOG(INFO) << "Total " << operations.size()
            << " allocate/free operations take "
            << (base::Time::Now() - start_time).InMicroseconds()
            << " microseconds.";
  // Logging the time since the last allocate operation, as there are often >10k
  // free operations when playback finishes.  Measuring the time spent on tail
  // free operations allows to understand how long this may take when playback
  // finishes.
  LOG(INFO) << "The last " << free_operations_since_last_allocate
            << " free operations take "
            << (base::Time::Now() - last_allocate_time).InMicroseconds()
            << " microseconds.";
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

  for (size_t i = 0; i < sizeof(kConfigs) / sizeof(*kConfigs); ++i) {
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
      const auto& operations = ReadAllocationLogFile(std::get<0>(GetParam()));

      for (auto&& operation : operations) {
        if (operation.buffer_type != buffer_type) {
          continue;
        }

        if (operation.operation_type == Operation::Type::kAllocate) {
          CHECK_EQ(pointer_to_pointer_map.count(operation.pointer), 0u);

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
          CHECK_EQ(pointer_to_pointer_map.erase(operation.pointer), 1u);
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

INSTANTIATE_TEST_SUITE_P(
    DecoderBufferAllocatorTests,
    DecoderBufferAllocatorTest,
    Combine(Values("starboard/allocations_1La4QzGeaaQ.txt",
                   "starboard/allocations_8k.txt",
                   "starboard/allocations_PMwaIrjiz8w.txt"),
            Bool()),
    [](const ::testing::TestParamInfo<std::tuple<std::string, bool>>& info) {
      std::string name = std::get<0>(info.param);
      std::replace(name.begin(), name.end(), '.', '_');
      std::replace(name.begin(), name.end(), '/', '_');

      name += std::get<1>(info.param) ? "_in_place_allocator"
                                      : "_default_allocator";
      return name;
    });

}  // namespace
}  // namespace media

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

#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "media/base/demuxer_stream.h"
#include "media/base/media_switches.h"
#include "media/base/starboard/sbmedia_histograms.h"
#include "media/base/test_data_util.h"
#include "starboard/common/pointer_arithmetic.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO: b/369245553 - Cobalt: As of today the tests here are used to experiment
//                     different allocation patterns.  Consider setting a real
//                     limit and fail allocation patterns taking excessive
//                     memory.

namespace media {
namespace {

typedef DecoderBufferAllocator::Handle Handle;
using ::testing::Values;

struct Operation {
  enum class Type { kAllocate, kFree } operation_type;
  std::string handle;
  DemuxerStream::Type buffer_type;
  int size;
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
    std::unordered_set<std::string> handles;
    for (auto&& allocation : allocations) {
      auto tokens = base::SplitStringUsingSubstr(
          allocation, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
      CHECK_EQ(tokens.size(), 4u);

      const std::string& handle = tokens[1];
      const DemuxerStream::Type buffer_type =
          static_cast<DemuxerStream::Type>(StringToInt(tokens[2]));
      const int size = StringToInt(tokens[3]);

      CHECK(buffer_type == DemuxerStream::UNKNOWN ||
            buffer_type == DemuxerStream::AUDIO ||
            buffer_type == DemuxerStream::VIDEO);

      if (tokens[0] == "allocate") {
        // In the format of "allocate <handle> <buffer_type> <size>"
        CHECK_EQ(handles.count(handle), 0u);

        handles.insert(handle);

        operations.emplace_back(
            Operation{Operation::Type::kAllocate, handle, buffer_type, size});
      } else {
        // In the format of "free <handle> <buffer_type> <size>"
        CHECK_EQ(tokens[0], "free");
        CHECK_EQ(handles.erase(handle), 1u);

        operations.emplace_back(
            Operation{Operation::Type::kFree, handle, buffer_type, size});
      }
    }

    iter =
        name_to_operations_map.insert(std::make_pair(name, operations)).first;
  }

  return iter->second;
}

class DecoderBufferAllocatorTest
    : public ::testing::TestWithParam<std::string> {};

TEST_P(DecoderBufferAllocatorTest, VerifyAndCacheAllocationLogs) {
  ReadAllocationLogFile(GetParam());
}

TEST_P(DecoderBufferAllocatorTest, CapacityUnderLimit) {
  DecoderBufferAllocator allocator;
  std::unordered_map<std::string, Handle> handle_to_handle_map;
  size_t max_allocated = 0, max_capacity = 0;
  base::Time start_time = base::Time::Now();
  base::Time last_allocate_time = start_time;
  int free_operations_since_last_allocate = 0;

  const auto& operations = ReadAllocationLogFile(GetParam());

  for (auto&& operation : operations) {
    if (operation.operation_type == Operation::Type::kAllocate) {
      CHECK_EQ(handle_to_handle_map.count(operation.handle), 0u);

      Handle h = allocator.Allocate(operation.buffer_type, operation.size);

      handle_to_handle_map[operation.handle] = h;
      max_allocated = std::max(max_allocated, allocator.GetAllocatedMemory());
      max_capacity =
          std::max(max_capacity, allocator.GetCurrentMemoryCapacity());
      last_allocate_time = base::Time::Now();
      free_operations_since_last_allocate = 0;
    } else {
      CHECK_EQ(operation.operation_type, Operation::Type::kFree);

      allocator.Free(operation.buffer_type,
                     handle_to_handle_map[operation.handle], operation.size);
      ++free_operations_since_last_allocate;

      CHECK_EQ(handle_to_handle_map.erase(operation.handle), 1u);
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
          /*is_memory_pool_allocated_on_demand=*/false,
          kConfigs[i].initial_capacity[buffer_type],
          kConfigs[i].allocation_increment[buffer_type]);
      std::unordered_map<std::string, Handle> handle_to_handle_map;
      size_t max_allocated = 0;
      size_t max_capacity = 0;
      const auto& operations = ReadAllocationLogFile(GetParam());

      for (auto&& operation : operations) {
        if (operation.buffer_type != buffer_type) {
          continue;
        }

        if (operation.operation_type == Operation::Type::kAllocate) {
          CHECK_EQ(handle_to_handle_map.count(operation.handle), 0u);

          handle_to_handle_map[operation.handle] =
              allocator.Allocate(operation.buffer_type, operation.size);
          max_allocated =
              std::max(max_allocated, allocator.GetAllocatedMemory());
          max_capacity =
              std::max(max_capacity, allocator.GetCurrentMemoryCapacity());
        } else {
          CHECK_EQ(operation.operation_type, Operation::Type::kFree);

          allocator.Free(operation.buffer_type,
                         handle_to_handle_map[operation.handle],
                         operation.size);
          CHECK_EQ(handle_to_handle_map.erase(operation.handle), 1u);
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

INSTANTIATE_TEST_SUITE_P(DecoderBufferAllocatorTests,
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

TEST(DecoderBufferAllocatorNonParameterizedTest,
     IsBufferPoolAllocateOnDemandMetric) {
  base::HistogramTester histogram_tester;

  {
    DecoderBufferAllocator allocator(
        /*is_memory_pool_allocated_on_demand=*/true, 0, 0);
    histogram_tester.ExpectBucketCount(kIsBufferPoolAllocateOnDemandHistogram,
                                       true, 1);
  }

  {
    DecoderBufferAllocator allocator(
        /*is_memory_pool_allocated_on_demand=*/false, 1024, 1024);
    histogram_tester.ExpectBucketCount(kIsBufferPoolAllocateOnDemandHistogram,
                                       false, 1);
  }
}

TEST(DecoderBufferAllocatorNonParameterizedTest, DisabledViaFeatureFlag) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      media::kCobaltDisableDecoderBufferAllocator);

  DecoderBufferAllocator allocator;
  EXPECT_EQ(allocator.GetAllocatedMemory(), 0u);
  EXPECT_EQ(allocator.GetCurrentMemoryCapacity(), 0u);
}

bool IsInteriorPageResident(Handle handle, size_t buffer_size) {
  const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
  uint8_t* raw_ptr = reinterpret_cast<uint8_t*>(handle);
  uint8_t* aligned_page = starboard::AlignUp(raw_ptr + page_size, page_size);
  CHECK_LE(aligned_page + page_size, raw_ptr + buffer_size);
  unsigned char vec = 0;
  CHECK_EQ(mincore(aligned_page, page_size, &vec), 0);
  return (vec & 1) != 0;
}

TEST(DecoderBufferAllocatorNonParameterizedTest,
     DecommitsIdleGrowthBlocksAboveRetainedWhenEmpty) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kCobaltDecoderBufferDecommitOnMemoryPressure,
      {{"CobaltDecoderBufferDecommitOnMemoryPressure_retain_blocks", "1"},
       {"CobaltDecoderBufferDecommitOnMemoryPressure_when_empty", "true"}});

  constexpr int kBlockSize = 256 * 1024;
  constexpr size_t kBufferSize = 192 * 1024;
  DecoderBufferAllocator allocator(
      /*is_memory_pool_allocated_on_demand=*/false, kBlockSize, kBlockSize);

  // Allocate 3 buffers so the pool has 1 initial block + 2 growth blocks.
  Handle h0 = allocator.Allocate(DemuxerStream::VIDEO, kBufferSize);
  Handle h1 = allocator.Allocate(DemuxerStream::VIDEO, kBufferSize);
  Handle h2 = allocator.Allocate(DemuxerStream::VIDEO, kBufferSize);
  ASSERT_NE(h0, DecoderBuffer::Allocator::kInvalidHandle);
  ASSERT_NE(h1, DecoderBuffer::Allocator::kInvalidHandle);
  ASSERT_NE(h2, DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_EQ(allocator.GetCurrentMemoryCapacity(),
            static_cast<size_t>(3 * kBlockSize));

  // Dirty all 3 blocks so OS pages are resident in RAM.
  std::memset(reinterpret_cast<void*>(h0), 0xAB, kBufferSize);
  std::memset(reinterpret_cast<void*>(h1), 0xAB, kBufferSize);
  std::memset(reinterpret_cast<void*>(h2), 0xAB, kBufferSize);
  ASSERT_TRUE(IsInteriorPageResident(h0, kBufferSize));
  ASSERT_TRUE(IsInteriorPageResident(h1, kBufferSize));
  ASSERT_TRUE(IsInteriorPageResident(h2, kBufferSize));

  // Free growth-block buffers while h0 is still live: pool is not empty yet, so
  // growth blocks remain committed.
  allocator.Free(DemuxerStream::VIDEO, h2, kBufferSize);
  allocator.Free(DemuxerStream::VIDEO, h1, kBufferSize);
  EXPECT_GT(allocator.GetAllocatedMemory(), 0u);
  EXPECT_TRUE(IsInteriorPageResident(h1, kBufferSize));
  EXPECT_TRUE(IsInteriorPageResident(h2, kBufferSize));

  // Freeing the last live buffer drops GetAllocatedMemory() to 0 and
  // immediately decommits idle growth blocks above the 1 retained block.
  allocator.Free(DemuxerStream::VIDEO, h0, kBufferSize);
  EXPECT_EQ(allocator.GetAllocatedMemory(), 0u);
  EXPECT_TRUE(IsInteriorPageResident(h0, kBufferSize));
  EXPECT_FALSE(IsInteriorPageResident(h1, kBufferSize));
  EXPECT_FALSE(IsInteriorPageResident(h2, kBufferSize));
}

TEST(DecoderBufferAllocatorNonParameterizedTest,
     KeepsIdleGrowthBlocksCommittedWhenEmptyParamFalse) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      media::kCobaltDecoderBufferDecommitOnMemoryPressure,
      {{"CobaltDecoderBufferDecommitOnMemoryPressure_retain_blocks", "1"},
       {"CobaltDecoderBufferDecommitOnMemoryPressure_when_empty", "false"}});

  constexpr int kBlockSize = 256 * 1024;
  constexpr size_t kBufferSize = 192 * 1024;
  DecoderBufferAllocator allocator(
      /*is_memory_pool_allocated_on_demand=*/false, kBlockSize, kBlockSize);

  Handle h0 = allocator.Allocate(DemuxerStream::VIDEO, kBufferSize);
  Handle h1 = allocator.Allocate(DemuxerStream::VIDEO, kBufferSize);
  std::memset(reinterpret_cast<void*>(h0), 0xCD, kBufferSize);
  std::memset(reinterpret_cast<void*>(h1), 0xCD, kBufferSize);
  ASSERT_TRUE(IsInteriorPageResident(h0, kBufferSize));
  ASSERT_TRUE(IsInteriorPageResident(h1, kBufferSize));

  allocator.Free(DemuxerStream::VIDEO, h1, kBufferSize);
  allocator.Free(DemuxerStream::VIDEO, h0, kBufferSize);
  EXPECT_EQ(allocator.GetAllocatedMemory(), 0u);

  // Without CobaltDecoderBufferDecommitOnMemoryPressure_when_empty=true, growth
  // block stays resident until an explicit memory pressure signal arrives.
  EXPECT_TRUE(IsInteriorPageResident(h0, kBufferSize));
  EXPECT_TRUE(IsInteriorPageResident(h1, kBufferSize));
}

}  // namespace
}  // namespace media

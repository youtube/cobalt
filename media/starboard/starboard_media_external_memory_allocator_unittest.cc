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

#include "media/starboard/starboard_media_external_memory_allocator.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/starboard/decoder_buffer_allocator.h"
#include "media/starboard/media_pool_direct_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

class StarboardMediaExternalMemoryAllocatorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Ensure the singleton DecoderBufferAllocator is initialized.
    ASSERT_NE(DecoderBufferAllocator::Get(), nullptr);
  }
};

TEST_F(StarboardMediaExternalMemoryAllocatorTest, CopyFromValidSpan) {
  StarboardMediaExternalMemoryAllocator allocator;
  std::vector<uint8_t> input = {0x10, 0x20, 0x30, 0x40, 0x50};

  size_t initial_allocated =
      DecoderBufferAllocator::Get()->GetAllocatedMemory();

  auto ext_mem = allocator.CopyFrom(input);
  ASSERT_NE(ext_mem, nullptr);

  base::span<const uint8_t> span = ext_mem->Span();
  EXPECT_EQ(span.size(), input.size());
  EXPECT_TRUE(std::equal(span.begin(), span.end(), input.begin()));

  size_t allocated_after_copy =
      DecoderBufferAllocator::Get()->GetAllocatedMemory();
  EXPECT_GE(allocated_after_copy, initial_allocated + input.size());

  // Destroy the external memory wrapper and verify memory reclamation.
  ext_mem.reset();
  size_t allocated_after_free =
      DecoderBufferAllocator::Get()->GetAllocatedMemory();
  EXPECT_EQ(allocated_after_free, initial_allocated);
}

TEST_F(StarboardMediaExternalMemoryAllocatorTest,
       MultipleAllocationsAndReclamation) {
  StarboardMediaExternalMemoryAllocator allocator;
  std::vector<uint8_t> input1(1024, 0xAA);
  std::vector<uint8_t> input2(2048, 0xBB);

  size_t initial_allocated =
      DecoderBufferAllocator::Get()->GetAllocatedMemory();

  auto ext_mem1 = allocator.CopyFrom(input1);
  auto ext_mem2 = allocator.CopyFrom(input2);
  ASSERT_NE(ext_mem1, nullptr);
  ASSERT_NE(ext_mem2, nullptr);

  EXPECT_EQ(ext_mem1->Span().size(), 1024u);
  EXPECT_EQ(ext_mem2->Span().size(), 2048u);

  size_t allocated_both = DecoderBufferAllocator::Get()->GetAllocatedMemory();
  EXPECT_GE(allocated_both, initial_allocated + 3072u);

  // Note: Starboard's default EmbeddedMetadataReuseAllocatorBase defers
  // deallocation (batched free) until all allocated blocks are freed. Thus,
  // freeing only ext_mem1 may keep GetAllocatedMemory() unchanged until
  // ext_mem2 is also released.
  ext_mem1.reset();
  ext_mem2.reset();
  EXPECT_EQ(DecoderBufferAllocator::Get()->GetAllocatedMemory(),
            initial_allocated);
}

TEST_F(StarboardMediaExternalMemoryAllocatorTest, CopyFromMultipleParts) {
  StarboardMediaExternalMemoryAllocator allocator;

  // Deliberately odd sizes, so that every part but the first lands on an
  // unaligned offset within the block.
  std::vector<uint8_t> part1 = {0x01, 0x02, 0x03};
  std::vector<uint8_t> part2 = {0x04};
  std::vector<uint8_t> part3(1021, 0xCD);

  std::vector<base::span<const uint8_t>> parts = {part1, part2, part3};
  std::vector<uint8_t> expected;
  for (const auto& part : parts) {
    expected.insert(expected.end(), part.begin(), part.end());
  }

  auto ext_mem = allocator.CopyFrom(parts, DemuxerStream::VIDEO);
  ASSERT_NE(ext_mem, nullptr);

  base::span<const uint8_t> span = ext_mem->Span();
  ASSERT_EQ(span.size(), expected.size());
  EXPECT_TRUE(std::equal(span.begin(), span.end(), expected.begin()));
}

TEST_F(StarboardMediaExternalMemoryAllocatorTest, CopyFromSinglePart) {
  StarboardMediaExternalMemoryAllocator allocator;
  std::vector<uint8_t> input = {0x10, 0x20, 0x30, 0x40, 0x50};

  std::vector<base::span<const uint8_t>> parts = {input};

  auto ext_mem = allocator.CopyFrom(parts, DemuxerStream::AUDIO);
  ASSERT_NE(ext_mem, nullptr);

  base::span<const uint8_t> span = ext_mem->Span();
  ASSERT_EQ(span.size(), input.size());
  EXPECT_TRUE(std::equal(span.begin(), span.end(), input.begin()));
}

TEST_F(StarboardMediaExternalMemoryAllocatorTest, CopyFromPartsWithEmptyPart) {
  StarboardMediaExternalMemoryAllocator allocator;
  std::vector<uint8_t> part1 = {0xAA, 0xBB};
  std::vector<uint8_t> empty;
  std::vector<uint8_t> part3 = {0xCC};

  std::vector<base::span<const uint8_t>> parts = {part1, empty, part3};
  const std::vector<uint8_t> expected = {0xAA, 0xBB, 0xCC};

  auto ext_mem = allocator.CopyFrom(parts, DemuxerStream::AUDIO);
  ASSERT_NE(ext_mem, nullptr);

  base::span<const uint8_t> span = ext_mem->Span();
  ASSERT_EQ(span.size(), expected.size());
  EXPECT_TRUE(std::equal(span.begin(), span.end(), expected.begin()));
}

TEST_F(StarboardMediaExternalMemoryAllocatorTest,
       CopyFromPartsOfZeroTotalSize) {
  StarboardMediaExternalMemoryAllocator allocator;
  std::vector<uint8_t> empty1;
  std::vector<uint8_t> empty2;

  std::vector<base::span<const uint8_t>> parts = {empty1, empty2};

  auto ext_mem = allocator.CopyFrom(parts, DemuxerStream::AUDIO);
  ASSERT_NE(ext_mem, nullptr);
  EXPECT_TRUE(ext_mem->Span().empty());
}

TEST_F(StarboardMediaExternalMemoryAllocatorTest, CopyFromPartsReclaimsMemory) {
  StarboardMediaExternalMemoryAllocator allocator;
  std::vector<uint8_t> part1(1024, 0xAA);
  std::vector<uint8_t> part2(2048, 0xBB);

  std::vector<base::span<const uint8_t>> parts = {part1, part2};
  const size_t total_size = part1.size() + part2.size();

  size_t initial_allocated =
      DecoderBufferAllocator::Get()->GetAllocatedMemory();

  auto ext_mem = allocator.CopyFrom(parts, DemuxerStream::VIDEO);
  ASSERT_NE(ext_mem, nullptr);
  EXPECT_EQ(ext_mem->Span().size(), total_size);

  // The gathered frame is accounted for in the pool.
  EXPECT_GE(DecoderBufferAllocator::Get()->GetAllocatedMemory(),
            initial_allocated + total_size);

  ext_mem.reset();
  EXPECT_EQ(DecoderBufferAllocator::Get()->GetAllocatedMemory(),
            initial_allocated);
}

TEST_F(StarboardMediaExternalMemoryAllocatorTest,
       CopyFromReturnsNullWhenMapFailsUnderSoftFail) {
  // Replace the global DecoderBufferAllocator with an empty on-demand pool
  // while MediaPoolDirectMap::Map() always fails so TryAllocate() returns
  // kInvalidHandle on every non-empty request.
  struct ScopedFailingPool {
    explicit ScopedFailingPool()
        : prev_allocator(DecoderBuffer::Allocator::Get()) {
      MediaPoolDirectMap::SetEnabledForTesting(true);
      MediaPoolDirectMap::SetMapFailForTesting(true);
      DecoderBuffer::Allocator::Set(nullptr);
      pool.emplace(/*is_memory_pool_allocated_on_demand=*/true,
                   /*initial_capacity=*/0,
                   /*allocation_unit=*/4 * 1024 * 1024);
      DecoderBuffer::Allocator::Set(&*pool);
    }
    ~ScopedFailingPool() {
      DecoderBuffer::Allocator::Set(nullptr);
      pool.reset();
      DecoderBuffer::Allocator::Set(prev_allocator);
      MediaPoolDirectMap::SetMapFailForTesting(false);
      MediaPoolDirectMap::SetEnabledForTesting(std::nullopt);
    }
    DecoderBuffer::Allocator* prev_allocator;
    std::optional<DecoderBufferAllocator> pool;
  } scoped_pool;

  StarboardMediaExternalMemoryAllocator allocator;
  const std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  const std::vector<uint8_t> data2 = {0x05, 0x06};

  // Single-span overloads return nullptr on non-empty input.
  EXPECT_EQ(allocator.CopyFrom(data), nullptr);
  EXPECT_EQ(allocator.CopyFrom(data, DemuxerStream::VIDEO), nullptr);

  // Single-part and multi-part overloads also return nullptr on non-empty data.
  const std::vector<base::span<const uint8_t>> single_part = {data};
  EXPECT_EQ(allocator.CopyFrom(single_part, DemuxerStream::AUDIO), nullptr);
  const std::vector<base::span<const uint8_t>> multi_part = {data, data2};
  EXPECT_EQ(allocator.CopyFrom(multi_part, DemuxerStream::VIDEO), nullptr);

  // Empty span and all-empty parts do not need a pool allocation and still
  // succeed with a zero-size ExternalMemory wrapper.
  base::span<const uint8_t> empty;
  auto empty_mem = allocator.CopyFrom(empty);
  ASSERT_NE(empty_mem, nullptr);
  EXPECT_TRUE(empty_mem->Span().empty());

  const std::vector<base::span<const uint8_t>> empty_parts = {empty, empty};
  auto empty_parts_mem = allocator.CopyFrom(empty_parts, DemuxerStream::AUDIO);
  ASSERT_NE(empty_parts_mem, nullptr);
  EXPECT_TRUE(empty_parts_mem->Span().empty());
}

TEST_F(StarboardMediaExternalMemoryAllocatorTest,
       EnsureHeadroomTrueWithoutSoftFail) {
  MediaPoolDirectMap::SetEnabledForTesting(false);
  MediaPoolDirectMap::SetMapFailForTesting(true);
  const size_t base_blocks = MediaPoolDirectMap::BlockCountForTesting();

  StarboardMediaExternalMemoryAllocator allocator;
  EXPECT_TRUE(allocator.EnsureHeadroom(2 * 1024 * 1024));
  EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks);

  MediaPoolDirectMap::SetMapFailForTesting(false);
  MediaPoolDirectMap::SetEnabledForTesting(std::nullopt);
}

// Installs a fresh DecoderBufferAllocator as the global allocator with
// MediaPoolDirectMap forced on, and restores everything on destruction, even
// if an ASSERT returns early.
class ScopedSoftFailPool {
 public:
  ScopedSoftFailPool(bool on_demand, bool map_fail)
      : prev_allocator_(DecoderBuffer::Allocator::Get()) {
    MediaPoolDirectMap::SetEnabledForTesting(true);
    MediaPoolDirectMap::SetMapFailForTesting(map_fail);
    DecoderBuffer::Allocator::Set(nullptr);
    pool_.emplace(/*is_memory_pool_allocated_on_demand=*/on_demand,
                  /*initial_capacity=*/0,
                  /*allocation_unit=*/4 * 1024 * 1024);
    DecoderBuffer::Allocator::Set(&*pool_);
  }
  ScopedSoftFailPool(const ScopedSoftFailPool&) = delete;
  ScopedSoftFailPool& operator=(const ScopedSoftFailPool&) = delete;
  ~ScopedSoftFailPool() {
    DecoderBuffer::Allocator::Set(nullptr);
    pool_.reset();
    DecoderBuffer::Allocator::Set(prev_allocator_);
    MediaPoolDirectMap::SetMapFailForTesting(false);
    MediaPoolDirectMap::SetEnabledForTesting(std::nullopt);
  }

  DecoderBufferAllocator& pool() { return *pool_; }

 private:
  const raw_ptr<DecoderBuffer::Allocator> prev_allocator_;
  std::optional<DecoderBufferAllocator> pool_;
};

TEST_F(StarboardMediaExternalMemoryAllocatorTest,
       EnsureHeadroomFalseWhenMapFailsUnderSoftFailNoLeaks) {
  for (bool on_demand : {false, true}) {
    const size_t base_blocks = MediaPoolDirectMap::BlockCountForTesting();
    ScopedSoftFailPool scoped_pool(on_demand, /*map_fail=*/true);
    StarboardMediaExternalMemoryAllocator allocator;

    EXPECT_TRUE(allocator.EnsureHeadroom(0));

    // An empty pool that can't grow fails without keeping any block.
    EXPECT_FALSE(allocator.EnsureHeadroom(2 * 1024 * 1024));
    EXPECT_EQ(scoped_pool.pool().GetAllocatedMemory(), 0u);
    EXPECT_EQ(scoped_pool.pool().GetCurrentMemoryCapacity(), 0u);
    EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks);
  }
}

TEST_F(StarboardMediaExternalMemoryAllocatorTest,
       EnsureHeadroomTrueWhenMapWorksAndGrowsCapacity) {
  const size_t base_blocks = MediaPoolDirectMap::BlockCountForTesting();
  {
    // A persistent (not on-demand) pool keeps the blocks it grew.
    ScopedSoftFailPool scoped_pool(/*on_demand=*/false, /*map_fail=*/false);
    DecoderBufferAllocator& pool = scoped_pool.pool();
    constexpr size_t kNeeded = 6 * 1024 * 1024;
    StarboardMediaExternalMemoryAllocator allocator;
    EXPECT_TRUE(allocator.EnsureHeadroom(kNeeded));
    EXPECT_EQ(pool.GetAllocatedMemory(), 0u);
    EXPECT_GE(pool.GetCurrentMemoryCapacity(), kNeeded);
    EXPECT_GT(MediaPoolDirectMap::BlockCountForTesting(), base_blocks);

    // With Map() failing, headroom within the grown pool still succeeds, and
    // more than the pool holds fails without changing it.
    MediaPoolDirectMap::SetMapFailForTesting(true);
    const size_t blocks_before_fail =
        MediaPoolDirectMap::BlockCountForTesting();
    const size_t cap_before_fail = pool.GetCurrentMemoryCapacity();
    EXPECT_TRUE(allocator.EnsureHeadroom(2 * 1024 * 1024));
    EXPECT_FALSE(allocator.EnsureHeadroom(cap_before_fail + 4 * 1024 * 1024));
    EXPECT_EQ(pool.GetAllocatedMemory(), 0u);
    EXPECT_EQ(pool.GetCurrentMemoryCapacity(), cap_before_fail);
    EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), blocks_before_fail);
  }
  EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks);
}

TEST_F(StarboardMediaExternalMemoryAllocatorTest,
       EnsureHeadroomOnDemandPoolWithAndWithoutLiveBuffer) {
  const size_t base_blocks = MediaPoolDirectMap::BlockCountForTesting();
  {
    ScopedSoftFailPool scoped_pool(/*on_demand=*/true, /*map_fail=*/false);
    DecoderBufferAllocator& pool = scoped_pool.pool();
    StarboardMediaExternalMemoryAllocator allocator;

    // An idle on-demand pool is released again right after the check.
    EXPECT_TRUE(allocator.EnsureHeadroom(1024 * 1024));
    EXPECT_EQ(pool.GetAllocatedMemory(), 0u);
    EXPECT_EQ(pool.GetCurrentMemoryCapacity(), 0u);
    EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks);

    // With a live allocation held, the pool keeps the grown capacity, and the
    // check doesn't change how much is allocated.
    const std::vector<uint8_t> live_data(1024, 0x7E);
    auto live = allocator.CopyFrom(live_data);
    ASSERT_NE(live, nullptr);
    const size_t allocated = pool.GetAllocatedMemory();
    constexpr size_t kNeeded = 6 * 1024 * 1024;
    EXPECT_TRUE(allocator.EnsureHeadroom(kNeeded));
    EXPECT_GE(pool.GetCurrentMemoryCapacity(), kNeeded);
    EXPECT_EQ(pool.GetAllocatedMemory(), allocated);
    EXPECT_GT(MediaPoolDirectMap::BlockCountForTesting(), base_blocks);

    live.reset();
    EXPECT_EQ(pool.GetCurrentMemoryCapacity(), 0u);
  }
  EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks);
}

}  // namespace
}  // namespace media

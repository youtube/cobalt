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

#include "media/starboard/starboard_memory_allocator.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <optional>

#include "base/test/gtest_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/starboard/decoder_buffer_allocator.h"
#include "media/starboard/media_pool_direct_map.h"
#include "starboard/common/bidirectional_fit_reuse_allocator.h"
#include "starboard/common/embedded_metadata_reuse_allocator_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

// The media pool's allocation increment on Android.
constexpr size_t kPoolIncrement = 4 * 1024 * 1024;

class StarboardMemoryAllocatorTest : public ::testing::TestWithParam<bool> {
 protected:
  StarboardMemoryAllocatorTest() : allocator_(GetParam(), GetParam()) {}
  StarboardMemoryAllocator allocator_;
};

TEST_P(StarboardMemoryAllocatorTest, AllocateAndFree) {
  void* p = allocator_.Allocate(1024);
  EXPECT_NE(p, nullptr);
  // Write to the memory to ensure it's valid.
  memset(p, 0xAB, 1024);
  allocator_.Free(p);
}

TEST_P(StarboardMemoryAllocatorTest, AllocateZeroSize) {
  void* p = allocator_.Allocate(0);
  // Allocate(0) can return a nullptr or a unique non-null pointer.
  // In either case, it should be possible to free it.
  if (p) {
    allocator_.Free(p);
  }
}

TEST_P(StarboardMemoryAllocatorTest, AllocateWithAlignment) {
  const size_t alignment = 256;
  void* p = allocator_.Allocate(1024, alignment);
  EXPECT_NE(p, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % alignment, 0U);
  allocator_.Free(p);
}

TEST_P(StarboardMemoryAllocatorTest, AllocateWithZeroAlignment) {
  const size_t alignment = 0;
  void* p = allocator_.Allocate(1024, alignment);
  EXPECT_NE(p, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % sizeof(void*), 0U);
  allocator_.Free(p);
}

TEST_P(StarboardMemoryAllocatorTest, GetCapacityAndAllocatedShouldBeZero) {
  EXPECT_EQ(allocator_.GetCapacity(), 0U);
  EXPECT_EQ(allocator_.GetAllocated(), 0U);
  void* p = allocator_.Allocate(1024);
  EXPECT_NE(p, nullptr);
  // Should still be zero as this allocator doesn't track allocations.
  EXPECT_EQ(allocator_.GetCapacity(), 0U);
  EXPECT_EQ(allocator_.GetAllocated(), 0U);
  allocator_.Free(p);
}

INSTANTIATE_TEST_SUITE_P(EnableDecommit,
                         StarboardMemoryAllocatorTest,
                         ::testing::Bool());

TEST(StarboardMemoryAllocatorDecommitSpecificTest,
     AllocateForAlignmentUpdatesSizeWhenDecommitEnabled) {
  // These check the posix_memalign() path, which keeps the exact size.
  MediaPoolDirectMap::SetEnabledForTesting(false);
  StarboardMemoryAllocator allocator(/*enable_decommit=*/true,
                                     /*enable_page_alignment=*/true);

  const size_t page_size = sysconf(_SC_PAGESIZE);
  const size_t initial_size = page_size / 2;  // A size smaller than page size

  size_t size = initial_size;
  size_t alignment = 1;
  void* ptr = allocator.AllocateForAlignment(&size, alignment);

  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(size, page_size);  // Should be aligned up to page size

  allocator.Free(ptr);
  MediaPoolDirectMap::SetEnabledForTesting(std::nullopt);
}

TEST(StarboardMemoryAllocatorDecommitSpecificTest,
     AllocateForAlignmentDoesNotUpdateSizeWhenDecommitDisabled) {
  // These check the posix_memalign() path, which keeps the exact size.
  MediaPoolDirectMap::SetEnabledForTesting(false);
  StarboardMemoryAllocator allocator(/*enable_decommit=*/false,
                                     /*enable_page_alignment=*/false);

  const size_t page_size = sysconf(_SC_PAGESIZE);
  const size_t initial_size = page_size / 2;

  size_t size = initial_size;
  size_t alignment = 1;
  void* ptr = allocator.AllocateForAlignment(&size, alignment);

  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(size, initial_size);  // Should remain unchanged

  allocator.Free(ptr);
  MediaPoolDirectMap::SetEnabledForTesting(std::nullopt);
}

TEST(StarboardMemoryAllocatorContiguityTest,
     CanAllocateContiguouslyIsTrueWithoutSoftFail) {
  MediaPoolDirectMap::SetEnabledForTesting(false);
  StarboardMemoryAllocator allocator(/*enable_decommit=*/false,
                                     /*enable_page_alignment=*/false);
  EXPECT_TRUE(allocator.CanAllocateContiguously());
  MediaPoolDirectMap::SetEnabledForTesting(std::nullopt);
}

class StarboardMemoryAllocatorSoftFailTest : public ::testing::Test {
 protected:
  static constexpr size_t kMaxBucketedSize = 983040;

  void SetUp() override { MediaPoolDirectMap::SetEnabledForTesting(true); }
  void TearDown() override {
    MediaPoolDirectMap::SetMapFailAboveForTesting(std::nullopt);
    MediaPoolDirectMap::SetMapFailForTesting(false);
    MediaPoolDirectMap::SetEnabledForTesting(std::nullopt);
  }
};

TEST_F(StarboardMemoryAllocatorSoftFailTest,
       CanAllocateContiguouslyIsFalseUnderSoftFail) {
  StarboardMemoryAllocator allocator(/*enable_decommit=*/false,
                                     /*enable_page_alignment=*/false);
  EXPECT_FALSE(allocator.CanAllocateContiguously());
}

TEST_F(StarboardMemoryAllocatorSoftFailTest,
       SmallAndLargeBlocksBothRouteThroughMediaPoolDirectMap) {
  StarboardMemoryAllocator allocator(/*enable_decommit=*/false,
                                     /*enable_page_alignment=*/false,
                                     /*max_step_size=*/kPoolIncrement);
  const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));

  // Unrestricted MediaPoolDirectMap: a 64 KiB request steps up to the largest mid-size
  // block (2 MiB) so a degraded pool doesn't map one block per frame.
  size_t small_stepped_size = 64 * 1024;
  void* small_stepped_ptr =
      allocator.AllocateForAlignment(&small_stepped_size, 1);
  ASSERT_NE(small_stepped_ptr, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(small_stepped_ptr) % page_size, 0u);
  EXPECT_EQ(small_stepped_size, size_t{2} << 20);
  memset(small_stepped_ptr, 0x11, 4096);
  EXPECT_EQ(static_cast<const uint8_t*>(small_stepped_ptr)[0], 0x11);
  static_cast<uint8_t*>(small_stepped_ptr)[small_stepped_size - 1] = 0x12;
  EXPECT_EQ(
      static_cast<const uint8_t*>(small_stepped_ptr)[small_stepped_size - 1],
      0x12);
  EXPECT_TRUE(MediaPoolDirectMap::Unmap(small_stepped_ptr));

  // A 4 MiB request exceeds all step-down sizes and maps 4 MiB directly.
  size_t large_size = 4 * 1024 * 1024;
  void* large_ptr = allocator.AllocateForAlignment(&large_size, page_size);
  ASSERT_NE(large_ptr, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(large_ptr) % page_size, 0u);
  EXPECT_EQ(large_size, 4u * 1024 * 1024);
  memset(large_ptr, 0x22, 4096);
  EXPECT_EQ(static_cast<const uint8_t*>(large_ptr)[0], 0x22);
  EXPECT_TRUE(MediaPoolDirectMap::Unmap(large_ptr));
}

TEST_F(StarboardMemoryAllocatorSoftFailTest,
       StepDownSelectsLargestFittingMidSizeBlock) {
  StarboardMemoryAllocator allocator(/*enable_decommit=*/false,
                                     /*enable_page_alignment=*/false,
                                     /*max_step_size=*/kPoolIncrement);
  const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));

  // Ceiling at 1 MiB: 2 MiB fails, 1 MiB succeeds.
  MediaPoolDirectMap::SetMapFailAboveForTesting(size_t{1} << 20);
  size_t size_1m = 64 * 1024;
  void* ptr_1m = allocator.AllocateForAlignment(&size_1m, 1);
  ASSERT_NE(ptr_1m, nullptr);
  EXPECT_EQ(size_1m, size_t{1} << 20);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptr_1m) % page_size, 0u);
  static_cast<uint8_t*>(ptr_1m)[size_1m - 1] = 0xA1;
  EXPECT_EQ(static_cast<const uint8_t*>(ptr_1m)[size_1m - 1], 0xA1);
  EXPECT_TRUE(MediaPoolDirectMap::Unmap(ptr_1m));

  // Ceiling at 512 KiB: 2 MiB and 1 MiB fail, 512 KiB succeeds.
  MediaPoolDirectMap::SetMapFailAboveForTesting(size_t{512} << 10);
  size_t size_512k = 64 * 1024;
  void* ptr_512k = allocator.AllocateForAlignment(&size_512k, 1);
  ASSERT_NE(ptr_512k, nullptr);
  EXPECT_EQ(size_512k, size_t{512} << 10);
  EXPECT_TRUE(MediaPoolDirectMap::Unmap(ptr_512k));

  // Ceiling at 256 KiB: all step-down sizes fail; falls back to the exact
  // requested 64 KiB (page-aligned).
  MediaPoolDirectMap::SetMapFailAboveForTesting(size_t{256} << 10);
  size_t size_64k = 64 * 1024;
  void* ptr_64k = allocator.AllocateForAlignment(&size_64k, 1);
  ASSERT_NE(ptr_64k, nullptr);
  EXPECT_EQ(size_64k, ((64u * 1024 + page_size - 1) / page_size) * page_size);
  EXPECT_TRUE(MediaPoolDirectMap::Unmap(ptr_64k));

  // Unaligned request under 256 KiB ceiling rounds up to a whole page.
  size_t unaligned_size = 64 * 1024 + 1;
  const size_t expected_rounded =
      ((unaligned_size + page_size - 1) / page_size) * page_size;
  void* ptr_unaligned = allocator.AllocateForAlignment(&unaligned_size, 1);
  ASSERT_NE(ptr_unaligned, nullptr);
  EXPECT_EQ(unaligned_size, expected_rounded);
  EXPECT_TRUE(MediaPoolDirectMap::Unmap(ptr_unaligned));

  // Request of 1.5 MiB with ceiling at 1.6 MiB: 2 MiB fails, 1 MiB and 512 KiB
  // are skipped because they are <= *size, and MediaPoolDirectMap(1.5 MiB) succeeds.
  MediaPoolDirectMap::SetMapFailAboveForTesting(1600 * 1024);
  size_t size_1500k = 1500 * 1024;
  const size_t expected_1500k =
      ((size_1500k + page_size - 1) / page_size) * page_size;
  void* ptr_1500k = allocator.AllocateForAlignment(&size_1500k, 1);
  ASSERT_NE(ptr_1500k, nullptr);
  EXPECT_EQ(size_1500k, expected_1500k);
  EXPECT_TRUE(MediaPoolDirectMap::Unmap(ptr_1500k));
}

TEST_F(StarboardMemoryAllocatorSoftFailTest, StepUpNeverExceedsMaxStepSize) {
  const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));

  // A pool growing in 1 MiB units never gets a 2 MiB step.
  StarboardMemoryAllocator one_mib(/*enable_decommit=*/false,
                                   /*enable_page_alignment=*/false,
                                   /*max_step_size=*/size_t{1} << 20);
  size_t size = 64 * 1024;
  void* p = one_mib.AllocateForAlignment(&size, 1);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(size, size_t{1} << 20);
  one_mib.Free(p);

  // Without a step size, the block is the page-rounded request.
  StarboardMemoryAllocator exact(/*enable_decommit=*/false,
                                 /*enable_page_alignment=*/false);
  size = 64 * 1024 + 1;
  p = exact.AllocateForAlignment(&size, 1);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(size, ((64u * 1024 + 1 + page_size - 1) / page_size) * page_size);
  exact.Free(p);
}

TEST_F(StarboardMemoryAllocatorSoftFailTest,
       FreeUnmapsDirectBlocksWithSoftFailAlone) {
  StarboardMemoryAllocator allocator(/*enable_decommit=*/true,
                                     /*enable_page_alignment=*/true,
                                     /*max_step_size=*/kPoolIncrement);
  const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));

  // A 64 KiB request steps up to a 2 MiB block; 4 MiB maps as is.
  for (size_t req : {size_t{64 * 1024}, size_t{4 * 1024 * 1024}}) {
    size_t size = req;
    void* ptr = allocator.AllocateForAlignment(&size, 1);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(size, std::max(req, size_t{2} << 20));
    EXPECT_EQ(msync(ptr, page_size, MS_ASYNC), 0);

    allocator.Free(ptr);

    // Both the user range and its guard pages must be unmapped.
    errno = 0;
    EXPECT_EQ(msync(static_cast<uint8_t*>(ptr) - page_size, page_size,
                    MS_ASYNC),
              -1);
    EXPECT_EQ(errno, ENOMEM);
    errno = 0;
    EXPECT_EQ(msync(ptr, page_size, MS_ASYNC), -1);
    EXPECT_EQ(errno, ENOMEM);
    errno = 0;
    EXPECT_EQ(msync(static_cast<uint8_t*>(ptr) + size, page_size, MS_ASYNC),
              -1);
    EXPECT_EQ(errno, ENOMEM);
    EXPECT_FALSE(MediaPoolDirectMap::Unmap(ptr));
  }

  // Also test freeing a non-stepped exact small block (ceiling 256 KiB).
  MediaPoolDirectMap::SetMapFailAboveForTesting(size_t{256} << 10);
  size_t exact_small = 64 * 1024;
  void* small_ptr = allocator.AllocateForAlignment(&exact_small, 1);
  ASSERT_NE(small_ptr, nullptr);
  EXPECT_EQ(exact_small, 64u * 1024);
  allocator.Free(small_ptr);
  EXPECT_FALSE(MediaPoolDirectMap::Unmap(small_ptr));
  MediaPoolDirectMap::SetMapFailAboveForTesting(std::nullopt);

  // Foreign / non-direct pointers still fall through to free().
  void* heap_ptr = malloc(256);
  ASSERT_NE(heap_ptr, nullptr);
  allocator.Free(heap_ptr);

  size_t over_size = 64 * 1024;
  void* overaligned = allocator.AllocateForAlignment(&over_size, 4 * page_size);
  ASSERT_NE(overaligned, nullptr);
  EXPECT_FALSE(MediaPoolDirectMap::Unmap(overaligned));
  allocator.Free(overaligned);
}

TEST_F(StarboardMemoryAllocatorSoftFailTest,
       MediaPoolDirectMapFailureReturnsNullptrForBothSmallAndLarge) {
  {
    MediaPoolDirectMap::SetMapFailForTesting(true);

    // With step-ups enabled, every step fails too.
    StarboardMemoryAllocator allocator(/*enable_decommit=*/false,
                                       /*enable_page_alignment=*/false,
                                       /*max_step_size=*/kPoolIncrement);
    const size_t page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));

    size_t small_size = 64 * 1024;
    EXPECT_EQ(allocator.AllocateForAlignment(&small_size, 1), nullptr);

    size_t limit_size = kMaxBucketedSize;
    EXPECT_EQ(allocator.AllocateForAlignment(&limit_size, 1), nullptr);

    size_t boundary_size = kMaxBucketedSize + 1;
    EXPECT_EQ(allocator.AllocateForAlignment(&boundary_size, 1), nullptr);

    size_t large_size = 4 * 1024 * 1024;
    EXPECT_EQ(allocator.AllocateForAlignment(&large_size, page_size), nullptr);
  }
}

TEST_F(StarboardMemoryAllocatorSoftFailTest,
       ReuseAllocatorRetryStepsDownThroughMediaPoolDirectMapWhenIncrementFails) {
  constexpr size_t kInitialCapacity = 4 * 1024 * 1024;
  constexpr size_t kSmallThreshold = 512;
  constexpr size_t kAllocationIncrement = 4 * 1024 * 1024;
  constexpr size_t kSmallRequest = 64 * 1024;

  const size_t base_blocks = MediaPoolDirectMap::BlockCountForTesting();
  {
    StarboardMemoryAllocator fallback(/*enable_decommit=*/false,
                                      /*enable_page_alignment=*/false,
                                      /*max_step_size=*/kPoolIncrement);
    starboard::BidirectionalFitReuseAllocator<
        starboard::EmbeddedMetadataReuseAllocatorBase>
        pool(&fallback, kInitialCapacity, kSmallThreshold,
             kAllocationIncrement);
    ASSERT_EQ(pool.GetCapacity(), kInitialCapacity);
    ASSERT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks + 1);

    // Fill almost the entire initial 4 MiB block so the next 64 KiB allocation
    // triggers ExpandToFit().
    void* filler = pool.Allocate(kInitialCapacity - 1024);
    ASSERT_NE(filler, nullptr);

    // With a 1 MiB ceiling, the 4 MiB increment fails and ExpandToFit's retry
    // (~64 KiB + metadata) steps down to a 1 MiB MediaPoolDirectMap block.
    MediaPoolDirectMap::SetMapFailAboveForTesting(size_t{1} << 20);
    void* small = pool.Allocate(kSmallRequest);
    ASSERT_NE(small, nullptr);
    memset(small, 0xC7, kSmallRequest);
    EXPECT_EQ(static_cast<const uint8_t*>(small)[0], 0xC7);
    EXPECT_EQ(static_cast<const uint8_t*>(small)[kSmallRequest - 1], 0xC7);
    EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks + 2);
    EXPECT_EQ(pool.GetCapacity() - kInitialCapacity, size_t{1} << 20);

    // Exhaust the 1 MiB stepped block, then drop the ceiling below 512 KiB so
    // all step-down sizes fail and the retry maps only the page-rounded
    // request size.
    void* small_filler = pool.Allocate((size_t{1} << 20) - kSmallRequest - 1024);
    ASSERT_NE(small_filler, nullptr);
    const size_t cap_before_tiny = pool.GetCapacity();

    MediaPoolDirectMap::SetMapFailAboveForTesting(256 * 1024);
    void* tiny_retry = pool.Allocate(kSmallRequest);
    ASSERT_NE(tiny_retry, nullptr);
    memset(tiny_retry, 0x3A, kSmallRequest);
    EXPECT_EQ(static_cast<const uint8_t*>(tiny_retry)[kSmallRequest - 1], 0x3A);
    EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks + 3);
    const size_t tiny_growth = pool.GetCapacity() - cap_before_tiny;
    EXPECT_GE(tiny_growth, kSmallRequest);
    EXPECT_LT(tiny_growth, 256u * 1024);

    EXPECT_TRUE(pool.TryFree(tiny_retry));
    EXPECT_TRUE(pool.TryFree(small_filler));
    EXPECT_TRUE(pool.TryFree(small));
    EXPECT_TRUE(pool.TryFree(filler));
    EXPECT_EQ(pool.GetAllocated(), 0u);
  }
  // All three MediaPoolDirectMap blocks were unmapped when `pool` was destroyed.
  EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks);
}

TEST_F(StarboardMemoryAllocatorSoftFailTest,
       ExpandToFitSkipsTopUpBlockWhenSoftFailEnabled) {
  constexpr size_t kInitialCapacity = 4 * 1024 * 1024;
  constexpr size_t kSmallThreshold = 512;
  constexpr size_t kAllocationIncrement = 4 * 1024 * 1024;
  constexpr size_t kLeaveFreeTarget = 512 * 1024;
  constexpr size_t kRequest = 640 * 1024;

  const size_t base_blocks = MediaPoolDirectMap::BlockCountForTesting();
  {
    StarboardMemoryAllocator fallback(/*enable_decommit=*/false,
                                      /*enable_page_alignment=*/false,
                                      /*max_step_size=*/kPoolIncrement);
    ASSERT_FALSE(fallback.CanAllocateContiguously());

    starboard::BidirectionalFitReuseAllocator<
        starboard::EmbeddedMetadataReuseAllocatorBase>
        pool(&fallback, kInitialCapacity, kSmallThreshold,
             kAllocationIncrement);
    ASSERT_EQ(pool.GetCapacity(), kInitialCapacity);
    ASSERT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks + 1);

    // Leave a substantial (~512 KiB) free block at the end of `pool`, so
    // `free_blocks_` is non-empty. A top-up allocation for `kRequest` (640 KiB)
    // would only need ~128 KiB, which is well below the 512 KiB ceiling below.
    void* filler = pool.Allocate(kInitialCapacity - kLeaveFreeTarget);
    ASSERT_NE(filler, nullptr);
    const size_t remaining_free = pool.GetCapacity() - pool.GetAllocated();
    ASSERT_GT(remaining_free, 256u * 1024);
    ASSERT_LT(remaining_free, kRequest);

    // Fail both 4 MiB increment and 640 KiB request, while allowing blocks up
    // to 512 KiB. If ExpandToFit attempted its third "top-up" step
    // (~128 KiB <= 512 KiB), MediaPoolDirectMap would succeed and strand an extra block.
    MediaPoolDirectMap::SetMapFailAboveForTesting(512 * 1024);
    EXPECT_EQ(pool.Allocate(kRequest), nullptr);
    EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks + 1);
    EXPECT_EQ(pool.GetCapacity(), kInitialCapacity);

    EXPECT_TRUE(pool.TryFree(filler));
    EXPECT_EQ(pool.GetAllocated(), 0u);
  }
  EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks);
}

TEST_F(StarboardMemoryAllocatorSoftFailTest,
       DecoderBufferAllocatorTryAllocateReturnsInvalidHandleWhenMediaPoolDirectMapFails) {
  MediaPoolDirectMap::SetMapFailForTesting(true);
  DecoderBufferAllocator allocator(/*is_memory_pool_allocated_on_demand=*/true,
                                   /*initial_capacity=*/0,
                                   /*allocation_unit=*/4 * 1024 * 1024);

  auto handle = allocator.TryAllocate(DemuxerStream::VIDEO, 64 * 1024);
  EXPECT_EQ(handle, DecoderBuffer::Allocator::kInvalidHandle);
}

TEST_F(StarboardMemoryAllocatorSoftFailTest,
       DecoderBufferAllocatorNonZeroInitialCapacityTryAllocateWhenMediaPoolDirectMapFails) {
  // Start with a live 4 MiB pool, exhaust it, then make MediaPoolDirectMap always fail.
  DecoderBufferAllocator allocator(/*is_memory_pool_allocated_on_demand=*/true,
                                   /*initial_capacity=*/4 * 1024 * 1024,
                                   /*allocation_unit=*/4 * 1024 * 1024);
  constexpr size_t kFillSize = 4 * 1024 * 1024 - 1024;
  auto filler = allocator.TryAllocate(DemuxerStream::VIDEO, kFillSize);
  ASSERT_NE(filler, DecoderBuffer::Allocator::kInvalidHandle);

  MediaPoolDirectMap::SetMapFailForTesting(true);
  auto failed = allocator.TryAllocate(DemuxerStream::VIDEO, 64 * 1024);
  EXPECT_EQ(failed, DecoderBuffer::Allocator::kInvalidHandle);

  allocator.Free(DemuxerStream::VIDEO, filler, kFillSize);
}

TEST_F(StarboardMemoryAllocatorSoftFailTest,
       FailedTryAllocateReleasesIdleOnDemandPool) {
  const size_t base_blocks = MediaPoolDirectMap::BlockCountForTesting();
  DecoderBufferAllocator allocator(/*is_memory_pool_allocated_on_demand=*/true,
                                   /*initial_capacity=*/kPoolIncrement,
                                   /*allocation_unit=*/kPoolIncrement);
  // The pool's initial block maps, but the larger frame can't, so no handle
  // comes back and nothing would ever free the initial block.
  MediaPoolDirectMap::SetMapFailAboveForTesting(kPoolIncrement);
  EXPECT_EQ(allocator.TryAllocate(DemuxerStream::VIDEO, 2 * kPoolIncrement),
            DecoderBuffer::Allocator::kInvalidHandle);
  EXPECT_EQ(allocator.GetCurrentMemoryCapacity(), 0u);
  EXPECT_EQ(MediaPoolDirectMap::BlockCountForTesting(), base_blocks);
}

TEST_F(StarboardMemoryAllocatorSoftFailTest,
       InitialCapacityFailureStartsEmptyAndGrowsLater) {
  // With soft failure, a pool whose initial block can't be mapped starts
  // empty (no DCHECK) and grows once mapping works again.
  MediaPoolDirectMap::SetMapFailForTesting(true);
  DecoderBufferAllocator allocator(/*is_memory_pool_allocated_on_demand=*/false,
                                   /*initial_capacity=*/4 * 1024 * 1024,
                                   /*allocation_unit=*/4 * 1024 * 1024);
  EXPECT_EQ(allocator.TryAllocate(DemuxerStream::VIDEO, 64 * 1024),
            DecoderBuffer::Allocator::kInvalidHandle);
  MediaPoolDirectMap::SetMapFailForTesting(false);
  auto handle = allocator.TryAllocate(DemuxerStream::VIDEO, 64 * 1024);
  ASSERT_NE(handle, DecoderBuffer::Allocator::kInvalidHandle);
  allocator.Free(DemuxerStream::VIDEO, handle, 64 * 1024);
}

using StarboardMemoryAllocatorSoftFailDeathTest =
    StarboardMemoryAllocatorSoftFailTest;

TEST_F(StarboardMemoryAllocatorSoftFailDeathTest,
       DecoderBufferAllocatorAllocateChecksWhenMediaPoolDirectMapAlwaysFails) {
  MediaPoolDirectMap::SetMapFailForTesting(true);
  DecoderBufferAllocator allocator(/*is_memory_pool_allocated_on_demand=*/true,
                                   /*initial_capacity=*/0,
                                   /*allocation_unit=*/4 * 1024 * 1024);

  EXPECT_CHECK_DEATH(allocator.Allocate(DemuxerStream::VIDEO, 64 * 1024));
}

}  // namespace
}  // namespace media

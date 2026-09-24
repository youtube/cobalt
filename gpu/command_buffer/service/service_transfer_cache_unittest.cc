// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/service_transfer_cache.h"

#include "base/test/bind.h"
#include "base/time/time_override.h"
#include "build/build_config.h"
#include "cc/paint/raw_memory_transfer_cache_entry.h"
#include "gpu/config/gpu_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_COBALT)
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "gpu/config/gpu_finch_features.h"
#endif

namespace gpu {

constexpr int kDecoderId = 2;
constexpr auto kEntryType = cc::TransferCacheEntryType::kRawMemory;

std::unique_ptr<cc::ServiceTransferCacheEntry> CreateEntry(size_t size) {
  auto entry = std::make_unique<cc::ServiceRawMemoryTransferCacheEntry>();
  std::vector<uint8_t> data(size, 0u);
  entry->Deserialize(/*gr_context=*/nullptr, /*graphite_recorder=*/nullptr,
                     data);
  return entry;
}

TEST(ServiceTransferCacheTest, EnforcesOnPurgeMemory) {
  ServiceTransferCache cache{GpuPreferences(), base::RepeatingClosure()};
  uint32_t entry_id = 0u;
  size_t entry_size = 1024u;
  uint32_t number_of_entry = 4u;

  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
      CreateEntry(entry_size));
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size);
  cache.PurgeMemory(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
  EXPECT_EQ(cache.cache_size_for_testing(), 0u);

  cache.SetCacheSizeLimitForTesting(entry_size * number_of_entry);
  // Create 4 entries, all in the cache.
  for (uint32_t i = 0; i < number_of_entry; i++) {
    cache.CreateLocalEntry(
        ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
        CreateEntry(entry_size));
    EXPECT_EQ(cache.cache_size_for_testing(), entry_size * (i + 1));
  }

  // The 5th entry creates successfully. But the 1st entry will be purged due to
  // cache limits.
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
      CreateEntry(entry_size));
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size * 4);

  cache.PurgeMemory(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  // Only 1/4 of cache limits remains.
  EXPECT_EQ(cache.cache_size_for_testing(), entry_size);
}

TEST(ServiceTransferCache, MultipleDecoderUse) {
  ServiceTransferCache cache{GpuPreferences(), base::RepeatingClosure()};
  const uint32_t entry_id = 0u;
  const size_t entry_size = 1024u;

  // Decoder 1 entry.
  int decoder1 = 1;
  auto decoder_1_entry = CreateEntry(entry_size);
  auto* decoder_1_entry_ptr = decoder_1_entry.get();
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(decoder1, kEntryType, entry_id),
      std::move(decoder_1_entry));

  // Decoder 2 entry.
  int decoder2 = 2;
  auto decoder_2_entry = CreateEntry(entry_size);
  auto* decoder_2_entry_ptr = decoder_2_entry.get();
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(decoder2, kEntryType, entry_id),
      std::move(decoder_2_entry));

  EXPECT_EQ(decoder_1_entry_ptr, cache.GetEntry(ServiceTransferCache::EntryKey(
                                     decoder1, kEntryType, entry_id)));
  EXPECT_EQ(decoder_2_entry_ptr, cache.GetEntry(ServiceTransferCache::EntryKey(
                                     decoder2, kEntryType, entry_id)));
}

TEST(ServiceTransferCache, DeleteEntriesForDecoder) {
  ServiceTransferCache cache{GpuPreferences(), base::RepeatingClosure()};
  const size_t entry_size = 1024u;
  const size_t cache_size = 4 * entry_size;
  cache.SetCacheSizeLimitForTesting(cache_size);

  // Add 2 entries for decoder 1.
  cache.CreateLocalEntry(ServiceTransferCache::EntryKey(1, kEntryType, 1),
                         CreateEntry(entry_size));
  cache.CreateLocalEntry(ServiceTransferCache::EntryKey(1, kEntryType, 2),
                         CreateEntry(entry_size));

  // Add 2 entries for decoder 2.
  cache.CreateLocalEntry(ServiceTransferCache::EntryKey(2, kEntryType, 1),
                         CreateEntry(entry_size));
  cache.CreateLocalEntry(ServiceTransferCache::EntryKey(2, kEntryType, 2),
                         CreateEntry(entry_size));

  // Erase all entries for decoder 1.
  EXPECT_EQ(cache.entries_count_for_testing(), 4u);
  cache.DeleteAllEntriesForDecoder(1);
  EXPECT_EQ(cache.entries_count_for_testing(), 2u);
  EXPECT_NE(cache.GetEntry(ServiceTransferCache::EntryKey(2, kEntryType, 1)),
            nullptr);
  EXPECT_NE(cache.GetEntry(ServiceTransferCache::EntryKey(2, kEntryType, 2)),
            nullptr);
}

TEST(ServiceTransferCacheTest, PurgeEntryOnTimer) {
  static base::TimeTicks now_value = base::TimeTicks::Now();
  base::subtle::ScopedTimeClockOverrides time_override(
      nullptr, []() { return now_value; }, nullptr);

  bool flush_called = false;
  ServiceTransferCache cache{
      GpuPreferences(),
      base::BindLambdaForTesting([&]() { flush_called = true; })};

  uint32_t entry_id = 0u;
  size_t entry_size = 1024u;
  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, ++entry_id),
      CreateEntry(entry_size));
  EXPECT_EQ(cache.entries_count_for_testing(), 1u);

  now_value = now_value + base::Minutes(1);
  cache.PruneOldEntries();
  EXPECT_EQ(cache.entries_count_for_testing(), 0u);
  EXPECT_TRUE(flush_called);
}

#if BUILDFLAG(IS_COBALT)

// PurgeEntryOnTimer above calls PruneOldEntries() directly, so it never runs
// through MaybePostPruneOldEntries() and never sees the feature check. The two
// tests below go through the public CreateLocalEntry() path and let the real
// timer fire, which is the only way the gating itself gets covered.

// Control arm of the A/B: with the feature off no timer is ever started, so an
// unlocked entry stays resident no matter how long the cache idles.
TEST(ServiceTransferCacheTest, IdleEntryIsKeptWhenPruneFeatureDisabled) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndDisableFeature(
      features::kPruneOldTransferCacheEntries);
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  bool flush_called = false;
  ServiceTransferCache cache{
      GpuPreferences(),
      base::BindLambdaForTesting([&]() { flush_called = true; })};

  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, 1u),
      CreateEntry(1024u));
  ASSERT_EQ(cache.entries_count_for_testing(), 1u);

  // Well past kOldEntryPruneInterval (30s) and kOldEntryCutoffTimeDelta (25s).
  task_environment.FastForwardBy(base::Minutes(1));

  EXPECT_EQ(cache.entries_count_for_testing(), 1u);
  EXPECT_FALSE(flush_called);
}

// Treatment arm: the same idle entry is reclaimed, and the cache asks its owner
// to flush so the GPU-side memory releases.
TEST(ServiceTransferCacheTest, IdleEntryIsReclaimedWhenPruneFeatureEnabled) {
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitAndEnableFeature(features::kPruneOldTransferCacheEntries);
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  bool flush_called = false;
  ServiceTransferCache cache{
      GpuPreferences(),
      base::BindLambdaForTesting([&]() { flush_called = true; })};

  cache.CreateLocalEntry(
      ServiceTransferCache::EntryKey(kDecoderId, kEntryType, 1u),
      CreateEntry(1024u));
  ASSERT_EQ(cache.entries_count_for_testing(), 1u);

  task_environment.FastForwardBy(base::Minutes(1));

  EXPECT_EQ(cache.entries_count_for_testing(), 0u);
  EXPECT_TRUE(flush_called);
}

#endif  // BUILDFLAG(IS_COBALT)

}  // namespace gpu

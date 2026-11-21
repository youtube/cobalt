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

#include "cobalt/browser/metrics/cobalt_storage_metrics_helper.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

namespace {

constexpr CacheDirBucket kAllCacheDirBuckets[] = {
    CacheDirBucket::kHttpCache,      CacheDirBucket::kJsCodeCache,
    CacheDirBucket::kWasmCodeCache,  CacheDirBucket::kServiceWorker,
    CacheDirBucket::kGpuShaderCache, CacheDirBucket::kOther,
};

std::string BucketSizeHistogramName(CacheDirBucket bucket) {
  return base::StrCat({"Cobalt.Storage.DirCache.",
                       GetCacheDirBucketName(bucket), ".StartupSizeKB"});
}

// Writes a file of |size_bytes| at |relative_path| (which uses '/' as the
// separator) under |root|, creating its parent directories.
bool WriteFileOfSize(const base::FilePath& root,
                     std::string_view relative_path,
                     size_t size_bytes) {
  const base::FilePath path = root.AppendASCII(relative_path);
  return base::CreateDirectory(path.DirName()) &&
         base::WriteFile(path, std::string(size_bytes, 'x'));
}

// Populates |cache_dir| with every directory that maps to a bucket, plus
// entries that must be counted as "Other". Sizes are whole KB so that the
// per-bucket KB samples add up exactly.
void PopulateCacheDir(const base::FilePath& cache_dir) {
  constexpr size_t kKB = 1024;
  // HTTP cache: 8 + 16 = 24 KB.
  ASSERT_TRUE(WriteFileOfSize(cache_dir, "Cache/index", 8 * kKB));
  ASSERT_TRUE(WriteFileOfSize(cache_dir, "Cache/Cache_Data/0123_0", 16 * kKB));
  // JS code cache: 4 + 6 = 10 KB.
  ASSERT_TRUE(WriteFileOfSize(
      cache_dir, "Code Cache/js/index-dir/the-real-index", 4 * kKB));
  ASSERT_TRUE(WriteFileOfSize(cache_dir, "Code Cache/js/4567_0", 6 * kKB));
  // Wasm code cache: 3 KB.
  ASSERT_TRUE(WriteFileOfSize(cache_dir, "Code Cache/wasm/89ab_0", 3 * kKB));
  // Service Worker, which holds CacheStorage: 12 + 1 = 13 KB.
  ASSERT_TRUE(WriteFileOfSize(
      cache_dir, "Service Worker/CacheStorage/origin/cache/cdef_0", 12 * kKB));
  ASSERT_TRUE(
      WriteFileOfSize(cache_dir, "Service Worker/Database/CURRENT", 1 * kKB));
  // GPU shader caches: 5 + 2 = 7 KB.
  ASSERT_TRUE(WriteFileOfSize(cache_dir, "GrShaderCache/data_1", 5 * kKB));
  ASSERT_TRUE(WriteFileOfSize(cache_dir, "ShaderCache/data_1", 2 * kKB));
  // Other: 1 + 2 + 3 + 4 + 5 + 6 = 21 KB.
  // A file directly in the cache directory.
  ASSERT_TRUE(WriteFileOfSize(cache_dir, "Local State", 1 * kKB));
  // A directory that is not one of the known caches.
  ASSERT_TRUE(WriteFileOfSize(cache_dir, "font_cache/font.ttf", 2 * kKB));
  // Entries under "Code Cache" that are neither "js" nor "wasm".
  ASSERT_TRUE(
      WriteFileOfSize(cache_dir, "Code Cache/webui_js/0000_0", 3 * kKB));
  ASSERT_TRUE(WriteFileOfSize(cache_dir, "Code Cache/stray_file", 4 * kKB));
  // Names that only share a prefix with a known cache, or that match a known
  // cache's name at the wrong level.
  ASSERT_TRUE(WriteFileOfSize(cache_dir, "Cache2/index", 5 * kKB));
  ASSERT_TRUE(WriteFileOfSize(cache_dir, "js/Cache/wasm/file", 6 * kKB));
}

}  // namespace

TEST(CobaltStorageMetricsHelperTest, ParseProcIoCountersValid) {
  constexpr std::string_view kSampleProcIo =
      "rchar: 8800000000\n"
      "wchar: 120000000\n"
      "syscr: 3668187\n"
      "syscw: 450123\n"
      "read_bytes: 15728640\n"
      "write_bytes: 524288\n"
      "cancelled_write_bytes: 4096\n";

  ProcessIoCounters counters;
  ASSERT_TRUE(ParseProcIoCounters(kSampleProcIo, &counters));
  EXPECT_EQ(counters.read_bytes, 15728640);
  EXPECT_EQ(counters.write_bytes, 524288);
}

TEST(CobaltStorageMetricsHelperTest, ParseProcIoCountersMissingFields) {
  constexpr std::string_view kIncompleteProcIo =
      "rchar: 1024\n"
      "wchar: 2048\n"
      "read_bytes: 4096\n";

  ProcessIoCounters counters;
  EXPECT_FALSE(ParseProcIoCounters(kIncompleteProcIo, &counters));
  EXPECT_FALSE(ParseProcIoCounters("", &counters));
  EXPECT_FALSE(ParseProcIoCounters(kIncompleteProcIo, nullptr));
}

TEST(CobaltStorageMetricsHelperTest, RecordStartupStorageAndIoMetricsForPaths) {
  base::ScopedTempDir user_data_dir;
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(user_data_dir.CreateUniqueTempDir());
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());

  // Write 24 KB into user_data_dir and 64 KB into cache_dir.
  const std::string user_data_payload(24 * 1024, 'u');
  const std::string cache_payload(64 * 1024, 'c');
  ASSERT_TRUE(base::WriteFile(user_data_dir.GetPath().AppendASCII("Cookies"),
                              user_data_payload));
  ASSERT_TRUE(
      base::WriteFile(cache_dir.GetPath().AppendASCII("index"), cache_payload));

  const ProcessIoCounters io_counters{
      .read_bytes = 12 * 1024 * 1024,  // 12,288 KB
      .write_bytes = 350 * 1024,       // 350 KB
  };

  base::HistogramTester histogram_tester;
  const std::optional<CacheDirBucketSizes> cache_dir_sizes =
      RecordStartupStorageAndIoMetricsForPaths(
          user_data_dir.GetPath(), cache_dir.GetPath(), &io_counters);

  histogram_tester.ExpectUniqueSample("Cobalt.Storage.ProcessIo.StartupReadKB",
                                      12288, 1);
  histogram_tester.ExpectUniqueSample("Cobalt.Storage.ProcessIo.StartupWriteKB",
                                      350, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.UserData.StartupTotalSizeKB", 24, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.StartupTotalSizeKB", 64, 1);

  // A file directly in the cache directory belongs to the "Other" bucket. The
  // remaining buckets are still recorded, as empty.
  ASSERT_TRUE(cache_dir_sizes.has_value());
  EXPECT_EQ(cache_dir_sizes->TotalBytes(), 64 * 1024);
  for (CacheDirBucket bucket : kAllCacheDirBuckets) {
    const int expected_kb = bucket == CacheDirBucket::kOther ? 64 : 0;
    EXPECT_EQ((*cache_dir_sizes)[bucket], expected_kb * 1024)
        << GetCacheDirBucketName(bucket);
    histogram_tester.ExpectUniqueSample(BucketSizeHistogramName(bucket),
                                        expected_kb, 1);
  }
}

TEST(CobaltStorageMetricsHelperTest, CacheDirBucketNames) {
  EXPECT_EQ(std::size(kAllCacheDirBuckets), kCacheDirBucketCount);
  EXPECT_EQ(BucketSizeHistogramName(CacheDirBucket::kHttpCache),
            "Cobalt.Storage.DirCache.HttpCache.StartupSizeKB");
  EXPECT_EQ(BucketSizeHistogramName(CacheDirBucket::kJsCodeCache),
            "Cobalt.Storage.DirCache.JsCodeCache.StartupSizeKB");
  EXPECT_EQ(BucketSizeHistogramName(CacheDirBucket::kWasmCodeCache),
            "Cobalt.Storage.DirCache.WasmCodeCache.StartupSizeKB");
  EXPECT_EQ(BucketSizeHistogramName(CacheDirBucket::kServiceWorker),
            "Cobalt.Storage.DirCache.ServiceWorker.StartupSizeKB");
  EXPECT_EQ(BucketSizeHistogramName(CacheDirBucket::kGpuShaderCache),
            "Cobalt.Storage.DirCache.GpuShaderCache.StartupSizeKB");
  EXPECT_EQ(BucketSizeHistogramName(CacheDirBucket::kOther),
            "Cobalt.Storage.DirCache.Other.StartupSizeKB");
}

TEST(CobaltStorageMetricsHelperTest, ComputeCacheDirBucketSizes) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  ASSERT_NO_FATAL_FAILURE(PopulateCacheDir(cache_dir.GetPath()));

  const CacheDirBucketSizes sizes =
      ComputeCacheDirBucketSizes(cache_dir.GetPath());
  EXPECT_EQ(sizes[CacheDirBucket::kHttpCache], 24 * 1024);
  EXPECT_EQ(sizes[CacheDirBucket::kJsCodeCache], 10 * 1024);
  EXPECT_EQ(sizes[CacheDirBucket::kWasmCodeCache], 3 * 1024);
  EXPECT_EQ(sizes[CacheDirBucket::kServiceWorker], 13 * 1024);
  EXPECT_EQ(sizes[CacheDirBucket::kGpuShaderCache], 7 * 1024);
  EXPECT_EQ(sizes[CacheDirBucket::kOther], 21 * 1024);

  // The buckets add up to the size of the whole directory.
  EXPECT_EQ(sizes.TotalBytes(), 78 * 1024);
  EXPECT_EQ(sizes.TotalBytes(),
            base::ComputeDirectorySize(cache_dir.GetPath()));

  // A trailing separator on the directory does not change the result.
  const CacheDirBucketSizes sizes_with_separator =
      ComputeCacheDirBucketSizes(cache_dir.GetPath().AsEndingWithSeparator());
  EXPECT_EQ(sizes_with_separator.bytes, sizes.bytes);
}

TEST(CobaltStorageMetricsHelperTest, ComputeCacheDirBucketSizesEmptyOrMissing) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());

  EXPECT_EQ(ComputeCacheDirBucketSizes(cache_dir.GetPath()).TotalBytes(), 0);
  EXPECT_EQ(ComputeCacheDirBucketSizes(
                cache_dir.GetPath().AppendASCII("does_not_exist"))
                .TotalBytes(),
            0);
}

TEST(CobaltStorageMetricsHelperTest, RecordsCacheDirBucketSizeHistograms) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  ASSERT_NO_FATAL_FAILURE(PopulateCacheDir(cache_dir.GetPath()));

  base::HistogramTester histogram_tester;
  const std::optional<CacheDirBucketSizes> sizes =
      RecordStartupStorageAndIoMetricsForPaths(
          /*user_data_dir=*/base::FilePath(), cache_dir.GetPath(),
          /*io_counters=*/nullptr);

  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.StartupTotalSizeKB", 78, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.HttpCache.StartupSizeKB", 24, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.JsCodeCache.StartupSizeKB", 10, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.WasmCodeCache.StartupSizeKB", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.ServiceWorker.StartupSizeKB", 13, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.GpuShaderCache.StartupSizeKB", 7, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.Other.StartupSizeKB", 21, 1);

  // The returned byte counts are the ones that were recorded.
  ASSERT_TRUE(sizes.has_value());
  EXPECT_EQ(sizes->bytes,
            ComputeCacheDirBucketSizes(cache_dir.GetPath()).bytes);

  // Nothing else is recorded when there is no user data directory and no I/O
  // counters.
  histogram_tester.ExpectTotalCount(
      "Cobalt.Storage.UserData.StartupTotalSizeKB", 0);
  histogram_tester.ExpectTotalCount("Cobalt.Storage.ProcessIo.StartupReadKB",
                                    0);
}

TEST(CobaltStorageMetricsHelperTest, CacheDirBucketSizesAreRoundedDownToKB) {
  base::ScopedTempDir cache_dir;
  ASSERT_TRUE(cache_dir.CreateUniqueTempDir());
  // 1.5 KB + 0.75 KB + 1023 bytes = 3327 bytes in total.
  ASSERT_TRUE(WriteFileOfSize(cache_dir.GetPath(), "Cache/index", 1536));
  ASSERT_TRUE(WriteFileOfSize(cache_dir.GetPath(), "Code Cache/js/index", 768));
  ASSERT_TRUE(WriteFileOfSize(cache_dir.GetPath(), "Local State", 1023));

  base::HistogramTester histogram_tester;
  const std::optional<CacheDirBucketSizes> sizes =
      RecordStartupStorageAndIoMetricsForPaths(
          /*user_data_dir=*/base::FilePath(), cache_dir.GetPath(),
          /*io_counters=*/nullptr);

  // The byte counts add up exactly; each KB sample is rounded down on its own.
  ASSERT_TRUE(sizes.has_value());
  EXPECT_EQ((*sizes)[CacheDirBucket::kHttpCache], 1536);
  EXPECT_EQ((*sizes)[CacheDirBucket::kJsCodeCache], 768);
  EXPECT_EQ((*sizes)[CacheDirBucket::kOther], 1023);
  EXPECT_EQ(sizes->TotalBytes(), 3327);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.StartupTotalSizeKB", 3, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.HttpCache.StartupSizeKB", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.JsCodeCache.StartupSizeKB", 0, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.DirCache.Other.StartupSizeKB", 0, 1);
}

TEST(CobaltStorageMetricsHelperTest, NoCacheDirHistogramsWithoutCacheDir) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::HistogramTester histogram_tester;
  EXPECT_FALSE(RecordStartupStorageAndIoMetricsForPaths(
                   /*user_data_dir=*/base::FilePath(),
                   /*cache_dir=*/base::FilePath(), /*io_counters=*/nullptr)
                   .has_value());
  EXPECT_FALSE(RecordStartupStorageAndIoMetricsForPaths(
                   /*user_data_dir=*/base::FilePath(),
                   temp_dir.GetPath().AppendASCII("does_not_exist"),
                   /*io_counters=*/nullptr)
                   .has_value());

  EXPECT_TRUE(
      histogram_tester.GetTotalCountsForPrefix("Cobalt.Storage.").empty());
}

namespace {

std::string QuotaHistogramName(CacheDirBucket bucket) {
  return base::StrCat({"Cobalt.Storage.CacheQuota.",
                       GetCacheDirBucketName(bucket), ".EffectiveMaxSizeKB"});
}

std::string UtilizationHistogramName(CacheDirBucket bucket) {
  return base::StrCat({"Cobalt.Storage.CacheQuota.",
                       GetCacheDirBucketName(bucket),
                       ".StartupUtilizationPercent"});
}

// The default quotas on a 32 MiB budget, of which HTTP took the extra 8 MiB.
CacheQuotaAllocation TestCacheQuotas() {
  constexpr int64_t kMiB = 1024 * 1024;
  CacheQuotaAllocation quotas;
  quotas.sizes = {.http_cache = 17 * kMiB,
                  .js_code_cache = 5 * kMiB,
                  .wasm_code_cache = 1 * kMiB,
                  .cache_storage = 6 * kMiB,
                  .gpu_shader_cache = 2 * kMiB,
                  .other = 1 * kMiB};
  quotas.outcome = CacheQuotaOutcome::kUnderCommittedResolvedByHttp;
  quotas.platform_budget_bytes = 32 * kMiB;
  quotas.budget_bytes = 32 * kMiB;
  quotas.configured_bytes = 24 * kMiB;
  quotas.uncommitted_bytes = 8 * kMiB;
  return quotas;
}

}  // namespace

TEST(CobaltStorageMetricsHelperTest, RecordsCacheQuotaHistograms) {
  constexpr int64_t kMiB = 1024 * 1024;
  CacheDirBucketSizes sizes;
  sizes[CacheDirBucket::kHttpCache] = 17 * kMiB / 2;      // 50%
  sizes[CacheDirBucket::kJsCodeCache] = 5 * kMiB;         // 100%
  sizes[CacheDirBucket::kWasmCodeCache] = 0;              // 0%
  sizes[CacheDirBucket::kServiceWorker] = 9 * kMiB;       // 150%
  sizes[CacheDirBucket::kGpuShaderCache] = kMiB / 2 - 1;  // Just under 25%
  sizes[CacheDirBucket::kOther] = 20 * kMiB;              // 2000%

  base::HistogramTester histogram_tester;
  RecordCacheQuotaMetrics(TestCacheQuotas(), &sizes);

  histogram_tester.ExpectUniqueSample(
      QuotaHistogramName(CacheDirBucket::kHttpCache), 17 * 1024, 1);
  histogram_tester.ExpectUniqueSample(
      QuotaHistogramName(CacheDirBucket::kJsCodeCache), 5 * 1024, 1);
  histogram_tester.ExpectUniqueSample(
      QuotaHistogramName(CacheDirBucket::kWasmCodeCache), 1 * 1024, 1);
  // The CacheStorage quota belongs to the Service Worker directory.
  histogram_tester.ExpectUniqueSample(
      QuotaHistogramName(CacheDirBucket::kServiceWorker), 6 * 1024, 1);
  histogram_tester.ExpectUniqueSample(
      QuotaHistogramName(CacheDirBucket::kGpuShaderCache), 2 * 1024, 1);
  histogram_tester.ExpectUniqueSample(
      QuotaHistogramName(CacheDirBucket::kOther), 1 * 1024, 1);

  histogram_tester.ExpectUniqueSample(
      UtilizationHistogramName(CacheDirBucket::kHttpCache), 50, 1);
  histogram_tester.ExpectUniqueSample(
      UtilizationHistogramName(CacheDirBucket::kJsCodeCache), 100, 1);
  histogram_tester.ExpectUniqueSample(
      UtilizationHistogramName(CacheDirBucket::kWasmCodeCache), 0, 1);
  histogram_tester.ExpectUniqueSample(
      UtilizationHistogramName(CacheDirBucket::kServiceWorker), 150, 1);
  histogram_tester.ExpectUniqueSample(
      UtilizationHistogramName(CacheDirBucket::kGpuShaderCache), 24, 1);
  // Above the histogram's range, so it lands in the overflow bucket.
  histogram_tester.ExpectUniqueSample(
      UtilizationHistogramName(CacheDirBucket::kOther), 1000, 1);

  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.CacheQuota.PlatformBudgetKB", 32 * 1024, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.CacheQuota.AllocationOutcome",
      CacheQuotaOutcome::kUnderCommittedResolvedByHttp, 1);
  histogram_tester.ExpectUniqueSample("Cobalt.Storage.CacheQuota.UncommittedKB",
                                      8 * 1024, 1);
}

// The budget is reported as the platform declares it, and the outcome as it
// is, also when the feature is disabled.
TEST(CobaltStorageMetricsHelperTest, RecordsCacheQuotaBudgetAndOutcomeAsGiven) {
  CacheQuotaAllocation quotas = TestCacheQuotas();
  quotas.outcome = CacheQuotaOutcome::kFeatureDisabled;
  quotas.platform_budget_bytes = 16 * 1024 * 1024;
  quotas.budget_bytes = 24 * 1024 * 1024;
  quotas.uncommitted_bytes = 0;

  base::HistogramTester histogram_tester;
  RecordCacheQuotaMetrics(quotas, nullptr);

  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.CacheQuota.PlatformBudgetKB", 16 * 1024, 1);
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.CacheQuota.AllocationOutcome",
      CacheQuotaOutcome::kFeatureDisabled, 1);
  histogram_tester.ExpectUniqueSample("Cobalt.Storage.CacheQuota.UncommittedKB",
                                      0, 1);
}

TEST(CobaltStorageMetricsHelperTest, NoCacheQuotaUtilizationWithoutSizes) {
  base::HistogramTester histogram_tester;
  RecordCacheQuotaMetrics(TestCacheQuotas(), nullptr);

  for (CacheDirBucket bucket : kAllCacheDirBuckets) {
    SCOPED_TRACE(GetCacheDirBucketName(bucket));
    histogram_tester.ExpectTotalCount(QuotaHistogramName(bucket), 1);
    histogram_tester.ExpectTotalCount(UtilizationHistogramName(bucket), 0);
  }
  histogram_tester.ExpectTotalCount(
      "Cobalt.Storage.CacheQuota.PlatformBudgetKB", 1);
  histogram_tester.ExpectTotalCount(
      "Cobalt.Storage.CacheQuota.AllocationOutcome", 1);
  histogram_tester.ExpectTotalCount("Cobalt.Storage.CacheQuota.UncommittedKB",
                                    1);
}

// A quota of 0 means that the cache has no quota, so there is nothing to
// divide its size by.
TEST(CobaltStorageMetricsHelperTest, NoCacheQuotaUtilizationWithoutQuota) {
  CacheQuotaAllocation quotas = TestCacheQuotas();
  quotas.sizes.gpu_shader_cache = 0;
  CacheDirBucketSizes sizes;
  sizes[CacheDirBucket::kGpuShaderCache] = 1024 * 1024;

  base::HistogramTester histogram_tester;
  RecordCacheQuotaMetrics(quotas, &sizes);

  histogram_tester.ExpectUniqueSample(
      QuotaHistogramName(CacheDirBucket::kGpuShaderCache), 0, 1);
  histogram_tester.ExpectTotalCount(
      UtilizationHistogramName(CacheDirBucket::kGpuShaderCache), 0);
  histogram_tester.ExpectTotalCount(
      UtilizationHistogramName(CacheDirBucket::kHttpCache), 1);
}

TEST(CobaltStorageMetricsHelperTest, RecordSteadyStateWriteRateForInterval) {
  base::HistogramTester histogram_tester;
  const base::TimeTicks t0 = base::TimeTicks::Now();

  // An empty interval should not emit.
  EXPECT_FALSE(RecordSteadyStateWriteRateForInterval(
      /*baseline_write_bytes=*/100 * 1024, t0,
      /*current_write_bytes=*/400 * 1024, t0));
  // Time going backwards should not emit.
  EXPECT_FALSE(RecordSteadyStateWriteRateForInterval(
      /*baseline_write_bytes=*/100 * 1024, t0,
      /*current_write_bytes=*/400 * 1024, t0 - base::Minutes(1)));
  // The write counter going backwards should not emit.
  EXPECT_FALSE(RecordSteadyStateWriteRateForInterval(
      /*baseline_write_bytes=*/400 * 1024, t0,
      /*current_write_bytes=*/100 * 1024, t0 + base::Minutes(5)));
  histogram_tester.ExpectTotalCount(
      "Cobalt.Storage.ProcessIo.WriteRateKBPerMin", 0);

  // 600 KB written over 2 minutes -> 300 KB/min.
  EXPECT_TRUE(RecordSteadyStateWriteRateForInterval(
      /*baseline_write_bytes=*/100 * 1024, t0,
      /*current_write_bytes=*/700 * 1024, t0 + base::Minutes(2)));
  histogram_tester.ExpectUniqueSample(
      "Cobalt.Storage.ProcessIo.WriteRateKBPerMin", 300, 1);
}

}  // namespace cobalt

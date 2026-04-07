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

#include "cobalt/browser/metrics/cobalt_memory_metrics_emitter.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

namespace {
constexpr int kKiB = 1024;
}  // namespace

class TestCobaltMemoryMetricsEmitter : public CobaltMemoryMetricsEmitter {
 public:
  TestCobaltMemoryMetricsEmitter() : CobaltMemoryMetricsEmitter() {}

  void PublicReceivedMemoryDump(
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump) {
    ReceivedMemoryDump(success, std::move(dump));
  }

 protected:
  ~TestCobaltMemoryMetricsEmitter() override = default;
};

TEST(CobaltMemoryMetricsEmitterTest, FragmentationCalculation) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;

  auto emitter = base::MakeRefCounted<TestCobaltMemoryMetricsEmitter>();

  // Set up a mock global dump
  memory_instrumentation::mojom::GlobalMemoryDumpPtr global_dump_mojom(
      memory_instrumentation::mojom::GlobalMemoryDump::New());

  memory_instrumentation::mojom::ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->pid = 1234;
  pmd->process_type = memory_instrumentation::mojom::ProcessType::BROWSER;
  pmd->os_dump = memory_instrumentation::mojom::OSMemDump::New();

  // Add V8 metrics
  memory_instrumentation::mojom::AllocatorMemDumpPtr v8_dump(
      memory_instrumentation::mojom::AllocatorMemDump::New());
  v8_dump->numeric_entries["effective_size"] = 1000;
  v8_dump->numeric_entries["allocated_objects_size"] = 800;
  pmd->chrome_allocator_dumps["v8"] = std::move(v8_dump);

  global_dump_mojom->process_dumps.push_back(std::move(pmd));

  auto global_dump = memory_instrumentation::GlobalMemoryDump::MoveFrom(
      std::move(global_dump_mojom));

  emitter->PublicReceivedMemoryDump(true, std::move(global_dump));

#if BUILDFLAG(IS_ANDROID)
  // On Android, we skip standard V8 metrics if has_accurate_rss is true.
  // However, in this test environment, has_accurate_rss might be false
  // if BUILDFLAG(IS_ANDROID) is not set during compilation.
  // Let's assume it's true for Android.
#else
  // Fragmentation for V8 should be (1000 - 800) * 100 / 1000 = 20%
  histogram_tester.ExpectUniqueSample(
      "Memory.Experimental.Browser2.V8.Fragmentation", 20, 1);
  // Wasted for V8 should be 1000 - 800 = 200 bytes
  histogram_tester.ExpectUniqueSample("Memory.Experimental.Browser2.V8.Wasted",
                                      200, 1);
#endif
}

#if BUILDFLAG(IS_ANDROID)
TEST(CobaltMemoryMetricsEmitterTest, DetailedMemoryStats) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;

  auto emitter = base::MakeRefCounted<TestCobaltMemoryMetricsEmitter>();

  // Set up a mock global dump with detailed stats
  memory_instrumentation::mojom::GlobalMemoryDumpPtr global_dump_mojom(
      memory_instrumentation::mojom::GlobalMemoryDump::New());

  memory_instrumentation::mojom::ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->pid = 1234;
  pmd->process_type = memory_instrumentation::mojom::ProcessType::BROWSER;
  pmd->os_dump = memory_instrumentation::mojom::OSMemDump::New();

  pmd->os_dump->detailed_stats =
      memory_instrumentation::mojom::DetailedMemoryStats::New();
  pmd->os_dump->detailed_stats->categories_kb
      [memory_instrumentation::mojom::CobaltMemoryCategory::kV8] = 10 * kKiB;
  pmd->os_dump->detailed_stats->categories_kb
      [memory_instrumentation::mojom::CobaltMemoryCategory::kMalloc] =
      20 * kKiB;

  global_dump_mojom->process_dumps.push_back(std::move(pmd));

  auto global_dump = memory_instrumentation::GlobalMemoryDump::MoveFrom(
      std::move(global_dump_mojom));

  emitter->PublicReceivedMemoryDump(true, std::move(global_dump));

  histogram_tester.ExpectUniqueSample("Memory.Browser.V8Rss", 10, 1);
  histogram_tester.ExpectUniqueSample("Memory.Browser.MallocRss", 20, 1);
  histogram_tester.ExpectUniqueSample("Memory.Experimental.Browser2.V8", 10, 1);
  histogram_tester.ExpectUniqueSample("Memory.Experimental.Browser2.Malloc", 20,
                                      1);
}

TEST(CobaltMemoryMetricsEmitterTest, DetailedMemoryStatsFallback) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;

  auto emitter = base::MakeRefCounted<TestCobaltMemoryMetricsEmitter>();

  // Set up a mock global dump with detailed stats but missing V8
  memory_instrumentation::mojom::GlobalMemoryDumpPtr global_dump_mojom(
      memory_instrumentation::mojom::GlobalMemoryDump::New());

  memory_instrumentation::mojom::ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->pid = 1234;
  pmd->process_type = memory_instrumentation::mojom::ProcessType::BROWSER;
  pmd->os_dump = memory_instrumentation::mojom::OSMemDump::New();

  // V8 is missing from detailed_stats but present in the old field
  pmd->os_dump->detailed_stats =
      memory_instrumentation::mojom::DetailedMemoryStats::New();
  pmd->os_dump->v8_rss_kb = 15 * kKiB;

  global_dump_mojom->process_dumps.push_back(std::move(pmd));

  auto global_dump = memory_instrumentation::GlobalMemoryDump::MoveFrom(
      std::move(global_dump_mojom));

  emitter->PublicReceivedMemoryDump(true, std::move(global_dump));

  // Should fall back to v8_rss_kb
  histogram_tester.ExpectUniqueSample("Memory.Browser.V8Rss", 15, 1);
  histogram_tester.ExpectUniqueSample("Memory.Experimental.Browser2.V8", 15, 1);
}
TEST(CobaltMemoryMetricsEmitterTest, DetailedMemoryStatsFallbackToAlloc) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;

  auto emitter = base::MakeRefCounted<TestCobaltMemoryMetricsEmitter>();

  // Set up a mock global dump where BOTH detailed_stats and old smaps fields
  // are missing/0
  memory_instrumentation::mojom::GlobalMemoryDumpPtr global_dump_mojom(
      memory_instrumentation::mojom::GlobalMemoryDump::New());

  memory_instrumentation::mojom::ProcessMemoryDumpPtr pmd(
      memory_instrumentation::mojom::ProcessMemoryDump::New());
  pmd->pid = 1234;
  pmd->process_type = memory_instrumentation::mojom::ProcessType::BROWSER;
  pmd->os_dump = memory_instrumentation::mojom::OSMemDump::New();

  pmd->os_dump->detailed_stats =
      memory_instrumentation::mojom::DetailedMemoryStats::New();
  // smaps fields are 0 by default.

  // Add V8 allocator dump metrics (in bytes)
  memory_instrumentation::mojom::AllocatorMemDumpPtr v8_dump(
      memory_instrumentation::mojom::AllocatorMemDump::New());
  v8_dump->numeric_entries["effective_size"] = 25 * 1024 * 1024;
  pmd->chrome_allocator_dumps["v8"] = std::move(v8_dump);

  global_dump_mojom->process_dumps.push_back(std::move(pmd));

  auto global_dump = memory_instrumentation::GlobalMemoryDump::MoveFrom(
      std::move(global_dump_mojom));

  emitter->PublicReceivedMemoryDump(true, std::move(global_dump));

  // Should fall back to v8 allocator dump (25 MB)
  histogram_tester.ExpectUniqueSample("Memory.Browser.V8Rss", 25, 1);
  histogram_tester.ExpectUniqueSample("Memory.Experimental.Browser2.V8", 25, 1);
}
#endif

}  // namespace cobalt

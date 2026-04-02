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

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "build/build_config.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {

class TestCobaltMemoryMetricsEmitter : public CobaltMemoryMetricsEmitter {
 public:
  TestCobaltMemoryMetricsEmitter() = default;
  using CobaltMemoryMetricsEmitter::ReceivedMemoryDump;

 protected:
  ~TestCobaltMemoryMetricsEmitter() override = default;
};

class CobaltMemoryMetricsEmitterTest : public testing::Test {
 public:
  CobaltMemoryMetricsEmitterTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(CobaltMemoryMetricsEmitterTest, CollateResults) {
  base::HistogramTester histogram_tester;
  auto emitter = base::MakeRefCounted<TestCobaltMemoryMetricsEmitter>();

  memory_instrumentation::mojom::GlobalMemoryDumpPtr dump_ptr =
      memory_instrumentation::mojom::GlobalMemoryDump::New();

  // Browser process dump
  auto browser_dump = memory_instrumentation::mojom::ProcessMemoryDump::New();
  browser_dump->process_type =
      memory_instrumentation::mojom::ProcessType::BROWSER;
  browser_dump->os_dump = memory_instrumentation::mojom::OSMemDump::New();
  browser_dump->os_dump->private_footprint_kb = 10240;  // 10 MB
  browser_dump->os_dump->resident_set_kb = 20480;       // 20 MB
  browser_dump->os_dump->shared_footprint_kb = 5120;    // 5 MB

  // Normal value (kValue)
  auto malloc_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
  malloc_dump->numeric_entries["effective_size"] = 10 * 1024 * 1024;
  browser_dump->chrome_allocator_dumps["malloc"] = std::move(malloc_dump);

  // Fragmentation (kFragmentation) - Normal
  auto blink_gc_dump = memory_instrumentation::mojom::AllocatorMemDump::New();
  blink_gc_dump->numeric_entries["effective_size"] = 1000;
  blink_gc_dump->numeric_entries["allocated_objects_size"] = 600;
  browser_dump->chrome_allocator_dumps["blink_gc"] = std::move(blink_gc_dump);

  // Fragmentation (kFragmentation) - Clamping (Allocated > Effective)
  auto pa_fast_malloc_dump =
      memory_instrumentation::mojom::AllocatorMemDump::New();
  pa_fast_malloc_dump->numeric_entries["effective_size"] = 1000;
  pa_fast_malloc_dump->numeric_entries["allocated_objects_size"] = 1200;
  browser_dump
      ->chrome_allocator_dumps["partition_alloc/partitions/fast_malloc"] =
      std::move(pa_fast_malloc_dump);

  // Missing secondary metric for fragmentation - Should skip
  auto renderer_dump = memory_instrumentation::mojom::ProcessMemoryDump::New();
  renderer_dump->process_type =
      memory_instrumentation::mojom::ProcessType::RENDERER;
  renderer_dump->os_dump = memory_instrumentation::mojom::OSMemDump::New();
  auto renderer_blink_gc_dump =
      memory_instrumentation::mojom::AllocatorMemDump::New();
  renderer_blink_gc_dump->numeric_entries["effective_size"] = 1000;
  // missing "allocated_objects_size"
  renderer_dump->chrome_allocator_dumps["blink_gc"] =
      std::move(renderer_blink_gc_dump);
  dump_ptr->process_dumps.push_back(std::move(renderer_dump));

#if BUILDFLAG(IS_ANDROID)
  browser_dump->os_dump->detailed_stats_kb = {
      {"rss:partition_alloc", 16384},
  };
#endif

  dump_ptr->process_dumps.push_back(std::move(browser_dump));

  auto global_dump =
      memory_instrumentation::GlobalMemoryDump::MoveFrom(std::move(dump_ptr));

  emitter->ReceivedMemoryDump(true, std::move(global_dump));

  // Verify kValue
  histogram_tester.ExpectUniqueSample("Memory.Experimental.Browser2.Malloc", 10,
                                      1);

  // Verify kFragmentation - Normal ( (1000-600)/1000 = 40% )
  histogram_tester.ExpectUniqueSample(
      "Memory.Experimental.Browser2.BlinkGC.Fragmentation", 40, 1);

  // Verify kFragmentation - Clamping ( (1000-1200) -> 0% )
  histogram_tester.ExpectUniqueSample(
      "Memory.Experimental.Browser2.PartitionAlloc.Fragmentation.FastMalloc", 0,
      1);

  // Verify missing secondary metric for renderer blink_gc.Fragmentation was
  // skipped.
  histogram_tester.ExpectTotalCount(
      "Memory.Experimental.Renderer2.BlinkGC.Fragmentation", 0);

#if BUILDFLAG(IS_ANDROID)
  // Verify supplemental RSS (instead of override)
  histogram_tester.ExpectUniqueSample(
      "Memory.Experimental.Browser2.PartitionAlloc.Rss", 16, 1);
#endif
}

}  // namespace cobalt

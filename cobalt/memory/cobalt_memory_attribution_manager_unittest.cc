// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/memory/cobalt_memory_attribution_manager.h"

#include <atomic>
#include <thread>

#include "base/feature_list.h"
#include "base/memory/cobalt_memory_attribution_observer.h"
#include "base/memory/cobalt_memory_context.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "cobalt/browser/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace memory {

using base::memory::GetCurrentMemoryContext;
using base::memory::MemoryContext;
using base::memory::ScopedMemoryContext;

class CobaltMemoryAttributionManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        cobalt::features::kCobaltMemoryAttributionManager);
    manager_ = CobaltMemoryAttributionManager::Get();
    auto* observer = base::memory::CobaltMemoryAttributionObserver::Get();
    // Reset counters and snapshots for clean test state.
    for (size_t i = 0; i < static_cast<size_t>(MemoryContext::kCount); ++i) {
      observer->GetCounters()[i].value.store(0, std::memory_order_relaxed);
      manager_->last_snapshots_[i] = 0;
    }
    manager_->last_report_time_ = base::TimeTicks::Now();
  }

  void TearDown() override { manager_->Stop(); }

  void TriggerAllocationHook(size_t size) {
    base::memory::CobaltMemoryAttributionObserver::Get()->OnAllocation(
        base::allocator::dispatcher::AllocationNotificationData(
            nullptr, size, nullptr,
            base::allocator::dispatcher::AllocationSubsystem::
                kPartitionAllocator));
  }

  void TriggerReportUma() { manager_->ReportUma(); }

  uint64_t GetCounter(MemoryContext context) {
    return base::memory::CobaltMemoryAttributionObserver::Get()
        ->GetCounters()[static_cast<size_t>(context)]
        .value.load(std::memory_order_relaxed);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  CobaltMemoryAttributionManager* manager_;
};

TEST_F(CobaltMemoryAttributionManagerTest, ScopedMemoryContextRestoresContext) {
  EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kUnknown);

  {
    ScopedMemoryContext dom_context(MemoryContext::kDOM);
    EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kDOM);

    {
      ScopedMemoryContext media_context(MemoryContext::kMedia);
      EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kMedia);
    }

    EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kDOM);
  }

  EXPECT_EQ(GetCurrentMemoryContext(), MemoryContext::kUnknown);
}

TEST_F(CobaltMemoryAttributionManagerTest,
       AllocationHookIncrementsActiveContext) {
  EXPECT_EQ(GetCounter(MemoryContext::kUnknown), 0u);
  TriggerAllocationHook(1024);
  EXPECT_EQ(GetCounter(MemoryContext::kUnknown), 1024u);

  MemoryContext contexts[] = {
      MemoryContext::kDOM,
      MemoryContext::kLayout,
      MemoryContext::kMedia,
      MemoryContext::kScript,
      MemoryContext::kNetwork,
      MemoryContext::kGraphics,
      MemoryContext::kStorage,
      MemoryContext::kGraphicsCanvas,
      MemoryContext::kGraphicsCompositor,
      MemoryContext::kGraphicsGlyphs,
      MemoryContext::kScriptHeap,
      MemoryContext::kScriptJIT,
      MemoryContext::kScriptBindings,
      MemoryContext::kNetworkLoader,
      MemoryContext::kNetworkCache,
      MemoryContext::kBlinkDOM,
      MemoryContext::kBlinkStyle,
      MemoryContext::kBlinkParser,
      MemoryContext::kPlatformIPC,
      MemoryContext::kPlatformStarboard,
  };

  for (MemoryContext context : contexts) {
    EXPECT_EQ(GetCounter(context), 0u);
    ScopedMemoryContext scoped_context(context);
    TriggerAllocationHook(2048);
    EXPECT_EQ(GetCounter(context), 2048u);
  }
}

TEST_F(CobaltMemoryAttributionManagerTest, ReportUmaEmitsDeltas) {
  base::HistogramTester histogram_tester;

  struct {
    MemoryContext context;
    const char* histogram_name;
  } test_cases[] = {
      {MemoryContext::kDOM, "Memory.Cobalt.AllocationVolume.DOM"},
      {MemoryContext::kLayout, "Memory.Cobalt.AllocationVolume.Layout"},
      {MemoryContext::kMedia, "Memory.Cobalt.AllocationVolume.Media"},
      {MemoryContext::kScript, "Memory.Cobalt.AllocationVolume.Script"},
      {MemoryContext::kNetwork, "Memory.Cobalt.AllocationVolume.Network"},
      {MemoryContext::kGraphics, "Memory.Cobalt.AllocationVolume.Graphics"},
      {MemoryContext::kStorage, "Memory.Cobalt.AllocationVolume.Storage"},
      {MemoryContext::kGraphicsCanvas,
       "Memory.Cobalt.AllocationVolume.GraphicsCanvas"},
      {MemoryContext::kGraphicsCompositor,
       "Memory.Cobalt.AllocationVolume.GraphicsCompositor"},
      {MemoryContext::kGraphicsGlyphs,
       "Memory.Cobalt.AllocationVolume.GraphicsGlyphs"},
      {MemoryContext::kScriptHeap, "Memory.Cobalt.AllocationVolume.ScriptHeap"},
      {MemoryContext::kScriptJIT, "Memory.Cobalt.AllocationVolume.ScriptJIT"},
      {MemoryContext::kScriptBindings,
       "Memory.Cobalt.AllocationVolume.ScriptBindings"},
      {MemoryContext::kNetworkLoader,
       "Memory.Cobalt.AllocationVolume.NetworkLoader"},
      {MemoryContext::kNetworkCache,
       "Memory.Cobalt.AllocationVolume.NetworkCache"},
      {MemoryContext::kBlinkDOM, "Memory.Cobalt.AllocationVolume.BlinkDOM"},
      {MemoryContext::kBlinkStyle, "Memory.Cobalt.AllocationVolume.BlinkStyle"},
      {MemoryContext::kBlinkParser,
       "Memory.Cobalt.AllocationVolume.BlinkParser"},
      {MemoryContext::kPlatformIPC,
       "Memory.Cobalt.AllocationVolume.PlatformIPC"},
      {MemoryContext::kPlatformStarboard,
       "Memory.Cobalt.AllocationVolume.PlatformStarboard"},
  };

  for (const auto& test_case : test_cases) {
    {
      ScopedMemoryContext scoped_context(test_case.context);
      TriggerAllocationHook(15 * 1024 * 1024);  // 15 MB
    }
  }

  TriggerReportUma();

  for (const auto& test_case : test_cases) {
    histogram_tester.ExpectUniqueSample(test_case.histogram_name, 15360, 1);
  }
  histogram_tester.ExpectUniqueSample("Memory.Cobalt.AllocationVolume.Unknown",
                                      0, 1);

  // Trigger more allocations and report again to verify delta calculation.
  {
    ScopedMemoryContext dom_context(MemoryContext::kDOM);
    TriggerAllocationHook(5 * 1024 * 1024);  // 5 MB
  }

  TriggerReportUma();

  histogram_tester.ExpectBucketCount("Memory.Cobalt.AllocationVolume.DOM", 5120,
                                     1);
  histogram_tester.ExpectTotalCount("Memory.Cobalt.AllocationVolume.DOM", 2);
}

TEST_F(CobaltMemoryAttributionManagerTest,
       AllocationHookOnNewThreadDoesNotCrash) {
  // This test specifically verifies that calling OnAllocation on a newly
  // spawned thread does not trigger emutls recursion crashes or static
  // initialization reentrancy deadlocks on Android x86/ARM.
  std::atomic_bool thread_executed{false};
  std::thread new_thread([&]() {
    // Trigger the first allocation on this new thread.
    TriggerAllocationHook(1024);
    thread_executed.store(true, std::memory_order_relaxed);
  });
  new_thread.join();
  EXPECT_TRUE(thread_executed.load(std::memory_order_relaxed));
}

}  // namespace memory
}  // namespace cobalt

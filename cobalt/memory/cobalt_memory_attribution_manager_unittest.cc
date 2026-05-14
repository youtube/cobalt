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

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "cobalt/browser/features.h"
#include "partition_alloc/partition_alloc_allocation_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace memory {

class CobaltMemoryAttributionManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        cobalt::features::kCobaltMemoryAttributionManager);
    manager_ = CobaltMemoryAttributionManager::Get();
    // Reset counters and snapshots for clean test state.
    for (size_t i = 0; i < static_cast<size_t>(MemoryContext::kCount); ++i) {
      manager_->counters_[i].store(0, std::memory_order_relaxed);
      manager_->last_snapshots_[i] = 0;
    }
    manager_->last_report_time_ = base::TimeTicks::Now();
  }

  void TearDown() override { manager_->Stop(); }

  void TriggerAllocationHook(size_t size) {
    partition_alloc::AllocationNotificationData notification_data(nullptr, size,
                                                                  nullptr);
    manager_->AllocationHook(notification_data);
  }

  void TriggerReportUma() { manager_->ReportUma(); }

  uint64_t GetCounter(MemoryContext context) {
    return manager_->counters_[static_cast<size_t>(context)].load(
        std::memory_order_relaxed);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  CobaltMemoryAttributionManager* manager_;
};

TEST_F(CobaltMemoryAttributionManagerTest, ScopedMemoryContextRestoresContext) {
  EXPECT_EQ(g_current_memory_context, MemoryContext::kUnknown);

  {
    ScopedMemoryContext dom_context(MemoryContext::kDOM);
    EXPECT_EQ(g_current_memory_context, MemoryContext::kDOM);

    {
      ScopedMemoryContext media_context(MemoryContext::kMedia);
      EXPECT_EQ(g_current_memory_context, MemoryContext::kMedia);
    }

    EXPECT_EQ(g_current_memory_context, MemoryContext::kDOM);
  }

  EXPECT_EQ(g_current_memory_context, MemoryContext::kUnknown);
}

TEST_F(CobaltMemoryAttributionManagerTest,
       AllocationHookIncrementsActiveContext) {
  EXPECT_EQ(GetCounter(MemoryContext::kUnknown), 0u);
  EXPECT_EQ(GetCounter(MemoryContext::kDOM), 0u);

  TriggerAllocationHook(1024);
  EXPECT_EQ(GetCounter(MemoryContext::kUnknown), 1024u);
  EXPECT_EQ(GetCounter(MemoryContext::kDOM), 0u);

  {
    ScopedMemoryContext dom_context(MemoryContext::kDOM);
    TriggerAllocationHook(2048);
    EXPECT_EQ(GetCounter(MemoryContext::kUnknown), 1024u);
    EXPECT_EQ(GetCounter(MemoryContext::kDOM), 2048u);
  }
}

TEST_F(CobaltMemoryAttributionManagerTest, ReportUmaEmitsDeltas) {
  base::HistogramTester histogram_tester;

  {
    ScopedMemoryContext dom_context(MemoryContext::kDOM);
    TriggerAllocationHook(15 * 1024 * 1024);  // 15 MB
  }

  TriggerReportUma();

  histogram_tester.ExpectUniqueSample("Memory.Cobalt.AllocationVolume.DOM", 15,
                                      1);
  histogram_tester.ExpectUniqueSample("Memory.Cobalt.AllocationVolume.Unknown",
                                      0, 1);

  // Trigger more allocations and report again to verify delta calculation.
  {
    ScopedMemoryContext dom_context(MemoryContext::kDOM);
    TriggerAllocationHook(5 * 1024 * 1024);  // 5 MB
  }

  TriggerReportUma();

  histogram_tester.ExpectBucketCount("Memory.Cobalt.AllocationVolume.DOM", 5,
                                     1);
  histogram_tester.ExpectTotalCount("Memory.Cobalt.AllocationVolume.DOM", 2);
}

}  // namespace memory
}  // namespace cobalt

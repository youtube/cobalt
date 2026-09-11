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

#include "cobalt/memory/cobalt_system_memory_pressure_evaluator.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace memory {

namespace {

constexpr uint64_t kTestBudgetBytes = 200ULL * 1024 * 1024;  // 200 MB

class CobaltSystemMemoryPressureEvaluatorTest : public testing::Test {
 public:
  CobaltSystemMemoryPressureEvaluatorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    // Normal system: 1000 MB total, 500 MB available (50% free).
    sys_info_.total = 1000 * 1024;
    sys_info_.available = 500 * 1024;
    sys_info_.free = 500 * 1024;
    sys_info_.buffers = 0;
    sys_info_.cached = 0;

    // Normal process: 120 MB private dirty (60% of 200 MB budget).
    proc_info_.resident_set_bytes = 150ULL * 1024 * 1024;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_FUCHSIA)
    proc_info_.rss_anon_bytes = 120ULL * 1024 * 1024;
#endif

    monitor_ =
        std::make_unique<::memory_pressure::MultiSourceMemoryPressureMonitor>();

    evaluator_ = std::make_unique<CobaltSystemMemoryPressureEvaluator>(
        monitor_->CreateVoter(),
        base::BindRepeating(
            &CobaltSystemMemoryPressureEvaluatorTest::GetProcessMemoryInfo,
            base::Unretained(this)),
        base::BindRepeating(
            &CobaltSystemMemoryPressureEvaluatorTest::GetSystemMemoryInfo,
            base::Unretained(this)),
        base::BindRepeating(
            &CobaltSystemMemoryPressureEvaluatorTest::GetMediaAllowance,
            base::Unretained(this)),
        kTestBudgetBytes,
        /*moderate_budget_ratio=*/0.85f,
        /*critical_budget_ratio=*/0.95f,
        /*moderate_system_fraction=*/0.30f,
        /*critical_system_fraction=*/0.15f,
        /*poll_interval=*/base::Seconds(5),
        /*cooldown=*/base::Seconds(15));

    listener_ = std::make_unique<base::MemoryPressureListener>(
        FROM_HERE,
        base::BindRepeating(
            &CobaltSystemMemoryPressureEvaluatorTest::OnMemoryPressure,
            base::Unretained(this)));
  }

  void TearDown() override {
    listener_.reset();
    evaluator_.reset();
    monitor_.reset();
  }

  base::expected<base::ProcessMemoryInfo, base::ProcessUsageError>
  GetProcessMemoryInfo() {
    if (get_proc_info_should_fail_) {
      return base::unexpected(base::ProcessUsageError::kSystemError);
    }
    return proc_info_;
  }

  bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* info) {
    if (get_sys_info_should_fail_) {
      return false;
    }
    *info = sys_info_;
    return true;
  }

  uint64_t GetMediaAllowance() const { return media_allowance_bytes_; }

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level) {
    notifications_.push_back(level);
  }

  void SetProcessPrivateMemoryMB(uint64_t mb) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_FUCHSIA)
    proc_info_.rss_anon_bytes = mb * 1024 * 1024;
#endif
    proc_info_.resident_set_bytes = mb * 1024 * 1024;
  }

  void SetSystemAvailableMemoryMB(int available_mb) {
    sys_info_.available = available_mb * 1024;
    sys_info_.free = available_mb * 1024;
  }

  void SetMediaAllowanceMB(uint64_t mb) {
    media_allowance_bytes_ = mb * 1024ULL * 1024ULL;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ProcessMemoryInfo proc_info_;
  base::SystemMemoryInfoKB sys_info_;
  uint64_t media_allowance_bytes_ = 0;
  bool get_proc_info_should_fail_ = false;
  bool get_sys_info_should_fail_ = false;

  std::unique_ptr<::memory_pressure::MultiSourceMemoryPressureMonitor> monitor_;
  std::unique_ptr<CobaltSystemMemoryPressureEvaluator> evaluator_;
  std::unique_ptr<base::MemoryPressureListener> listener_;
  std::vector<base::MemoryPressureListener::MemoryPressureLevel> notifications_;
};

// -----------------------------------------------------------------------------
// Process Budget Tests
// -----------------------------------------------------------------------------

TEST_F(CobaltSystemMemoryPressureEvaluatorTest, NormalProcessMemory) {
  // Process at 120 MB (60% of 200 MB budget) -> NONE.
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator_->current_vote());
  EXPECT_TRUE(notifications_.empty());
}

TEST_F(CobaltSystemMemoryPressureEvaluatorTest, ProcessTransitionToModerate) {
  // Increase process to 175 MB (87.5% of 200 MB budget, >= 85%) -> MODERATE.
  SetProcessPrivateMemoryMB(175);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator_->current_vote());
  ASSERT_EQ(1u, notifications_.size());
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            notifications_.back());
}

TEST_F(CobaltSystemMemoryPressureEvaluatorTest, ProcessTransitionToCritical) {
  // Increase process to 195 MB (97.5% of 200 MB budget, >= 95%) -> CRITICAL.
  SetProcessPrivateMemoryMB(195);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator_->current_vote());
  ASSERT_EQ(1u, notifications_.size());
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            notifications_.back());
}

TEST_F(CobaltSystemMemoryPressureEvaluatorTest, ProcessLifecycleRecovery) {
  // 1. Moderate.
  SetProcessPrivateMemoryMB(175);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator_->current_vote());
  EXPECT_EQ(1u, notifications_.size());

  // 2. Critical.
  SetProcessPrivateMemoryMB(195);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator_->current_vote());
  EXPECT_EQ(2u, notifications_.size());

  // 3. Recover to 140 MB (70%).
  SetProcessPrivateMemoryMB(140);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator_->current_vote());
  // NONE does not notify listeners.
  EXPECT_EQ(2u, notifications_.size());
}

// -----------------------------------------------------------------------------
// System Memory Fallback Tests
// -----------------------------------------------------------------------------

TEST_F(CobaltSystemMemoryPressureEvaluatorTest, SystemFallbackModerate) {
  // Process is normal (120 MB), but system drops to 25% (< 30%) -> MODERATE.
  SetSystemAvailableMemoryMB(250);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator_->current_vote());
  ASSERT_EQ(1u, notifications_.size());
}

TEST_F(CobaltSystemMemoryPressureEvaluatorTest, SystemFallbackCritical) {
  // Process is normal (120 MB), but system drops to 10% (< 15%) -> CRITICAL.
  SetSystemAvailableMemoryMB(100);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator_->current_vote());
  ASSERT_EQ(1u, notifications_.size());
}

// -----------------------------------------------------------------------------
// Dual Heuristic Max Tests
// -----------------------------------------------------------------------------

TEST_F(CobaltSystemMemoryPressureEvaluatorTest, DualMaxProcessWins) {
  // Process is CRITICAL (195 MB), System is MODERATE (250 MB / 25%).
  SetProcessPrivateMemoryMB(195);
  SetSystemAvailableMemoryMB(250);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator_->current_vote());
}

TEST_F(CobaltSystemMemoryPressureEvaluatorTest, DualMaxSystemWins) {
  // Process is MODERATE (175 MB), System is CRITICAL (100 MB / 10%).
  SetProcessPrivateMemoryMB(175);
  SetSystemAvailableMemoryMB(100);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator_->current_vote());
}

// -----------------------------------------------------------------------------
// Cooldown & Resiliency Tests
// -----------------------------------------------------------------------------

TEST_F(CobaltSystemMemoryPressureEvaluatorTest,
       CooldownPreventsThrottlingSpam) {
  SetProcessPrivateMemoryMB(175);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, notifications_.size());

  // 5 seconds later: still Moderate, within 15s cooldown -> no new
  // notification.
  task_environment_.FastForwardBy(base::Seconds(5));
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, notifications_.size());

  // Fast-forward 10s more (15s total cooldown) -> renotify.
  task_environment_.FastForwardBy(base::Seconds(10));
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, notifications_.size());
}

TEST_F(CobaltSystemMemoryPressureEvaluatorTest,
       MediaAllowanceExpandsEffectiveBudget) {
  // Budget is 200 MB.
  // Set private memory to 250 MB (without allowance, 250 / 200 = 125% ->
  // CRITICAL).
  SetProcessPrivateMemoryMB(250);

  // Without media allowance -> CRITICAL.
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator_->current_vote());

  // Simulate 100 MB media buffer allocation (e.g. video playback started).
  // Effective budget becomes 200 MB + 100 MB = 300 MB.
  // 250 MB / 300 MB = 83.3% (< 85% MODERATE threshold) -> NONE.
  SetMediaAllowanceMB(100);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator_->current_vote());

  // If memory continues growing during playback to 270 MB (90% of 300 MB) ->
  // MODERATE.
  SetProcessPrivateMemoryMB(270);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator_->current_vote());

  // Playback stops, media allowance returns to 0.
  // Effective budget returns to 200 MB.
  // 270 MB / 200 MB = 135% -> CRITICAL.
  SetMediaAllowanceMB(0);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator_->current_vote());
}

TEST_F(CobaltSystemMemoryPressureEvaluatorTest, HandlesErrorsGracefully) {
  get_proc_info_should_fail_ = true;
  get_sys_info_should_fail_ = true;
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator_->current_vote());
  EXPECT_TRUE(notifications_.empty());
}

TEST_F(CobaltSystemMemoryPressureEvaluatorTest,
       ResolveProcessMemoryBudgetTiers) {
  // Low-end tier (<= 1 GB total RAM) -> 180 MB
  uint64_t low_end_ram = 1024ULL * 1024 * 1024;
  EXPECT_EQ(180ULL * 1024 * 1024,
            CobaltSystemMemoryPressureEvaluator::ResolveProcessMemoryBudget(
                low_end_ram));

  uint64_t stick_ram = 512ULL * 1024 * 1024;
  EXPECT_EQ(180ULL * 1024 * 1024,
            CobaltSystemMemoryPressureEvaluator::ResolveProcessMemoryBudget(
                stick_ram));

  // Standard tier (> 1 GB total RAM, e.g. 2 GB) -> 200 MB
  uint64_t standard_ram = 2ULL * 1024 * 1024 * 1024;
  EXPECT_EQ(200ULL * 1024 * 1024,
            CobaltSystemMemoryPressureEvaluator::ResolveProcessMemoryBudget(
                standard_ram));

  // 4 GB RAM -> 200 MB standard default
  uint64_t high_end_ram = 4ULL * 1024 * 1024 * 1024;
  EXPECT_EQ(200ULL * 1024 * 1024,
            CobaltSystemMemoryPressureEvaluator::ResolveProcessMemoryBudget(
                high_end_ram));
}

}  // namespace
}  // namespace memory
}  // namespace cobalt

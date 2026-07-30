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
#include "base/test/gtest_util.h"
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
    // Normal process: 120 MB private dirty (60% of 200 MB budget).
    proc_info_.resident_set_bytes = 150ULL * 1024 * 1024;
#if BUILDFLAG(IS_STARBOARD) || BUILDFLAG(IS_ANDROID)
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
            &CobaltSystemMemoryPressureEvaluatorTest::GetMediaAllowance,
            base::Unretained(this)),
        kTestBudgetBytes,
        /*moderate_process_memory_fraction=*/0.85f,
        /*critical_process_memory_fraction=*/0.95f,
        /*poll_interval=*/base::Seconds(10),
        /*cooldown=*/base::Seconds(60));

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

  uint64_t GetMediaAllowance() const { return media_allowance_bytes_; }

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level) {
    notifications_.push_back(level);
  }

  void SetProcessPrivateMemoryMB(uint64_t mb) {
#if BUILDFLAG(IS_STARBOARD) || BUILDFLAG(IS_ANDROID)
    proc_info_.rss_anon_bytes = mb * 1024 * 1024;
#endif
    proc_info_.resident_set_bytes = mb * 1024 * 1024;
  }

  void SetMediaAllowanceMB(uint64_t mb) {
    media_allowance_bytes_ = mb * 1024ULL * 1024ULL;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::ProcessMemoryInfo proc_info_;
  uint64_t media_allowance_bytes_ = 0;
  bool get_proc_info_should_fail_ = false;

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

  // 2. Critical (transitions immediately without cooldown delay).
  SetProcessPrivateMemoryMB(195);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator_->current_vote());
  EXPECT_EQ(2u, notifications_.size());
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            notifications_.back());

  // 3. Drop back to Moderate: old_vote != current_vote(), so notifies
  // immediately.
  SetProcessPrivateMemoryMB(175);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator_->current_vote());
  EXPECT_EQ(3u, notifications_.size());
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            notifications_.back());

  // 4. Recover to 140 MB (70%).
  SetProcessPrivateMemoryMB(140);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator_->current_vote());
  // NONE does not notify listeners.
  EXPECT_EQ(3u, notifications_.size());
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

  // 10 seconds later: still Moderate, within 60s cooldown -> no new
  // notification.
  task_environment_.FastForwardBy(base::Seconds(10));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, notifications_.size());

  // Fast-forward 50s more (60s total cooldown) -> renotify.
  task_environment_.FastForwardBy(base::Seconds(50));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, notifications_.size());
}

TEST_F(CobaltSystemMemoryPressureEvaluatorTest, CriticalAlwaysNotifies) {
  // 1. Initial transition to Critical fires notification immediately.
  SetProcessPrivateMemoryMB(195);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, notifications_.size());
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator_->current_vote());

  // 2. Next check while still Critical fires notification immediately again
  // (no cooldown suppression for critical emergency pressure).
  task_environment_.FastForwardBy(base::Seconds(10));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, notifications_.size());
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            evaluator_->current_vote());
}

TEST_F(CobaltSystemMemoryPressureEvaluatorTest,
       TransitionFromCriticalToModerateNotifiesImmediately) {
  // 1. Start in Critical.
  SetProcessPrivateMemoryMB(195);
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, notifications_.size());

  // 2. Step down to Moderate: transition resets cooldown and notifies
  // immediately.
  SetProcessPrivateMemoryMB(175);
  task_environment_.FastForwardBy(base::Seconds(10));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, notifications_.size());
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator_->current_vote());

  // 3. Sustained Moderate: suppressed within cooldown.
  task_environment_.FastForwardBy(base::Seconds(10));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, notifications_.size());

  // 4. Cooldown completes (60s): renotifies Moderate.
  task_environment_.FastForwardBy(base::Seconds(50));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, notifications_.size());
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
  evaluator_->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE,
            evaluator_->current_vote());
  EXPECT_TRUE(notifications_.empty());
}

TEST_F(CobaltSystemMemoryPressureEvaluatorTest,
       ResolveProcessMemoryBudgetTiers) {
  // Tier 1: Super low-end tier (<= 512 MB physical RAM) -> 120 MB
  uint64_t stick_ram = 512ULL * 1024 * 1024;
  EXPECT_EQ(120ULL * 1024 * 1024,
            CobaltSystemMemoryPressureEvaluator::ResolveProcessMemoryBudget(
                stick_ram));

  // Tier 2: Low-end tier (> 512 MB and <= 1024 MB physical RAM) -> 160 MB
  uint64_t low_end_ram = 1024ULL * 1024 * 1024;
  EXPECT_EQ(160ULL * 1024 * 1024,
            CobaltSystemMemoryPressureEvaluator::ResolveProcessMemoryBudget(
                low_end_ram));

  // Tier 3: Standard tier (> 1024 MB and <= 2048 MB physical RAM, e.g. 1.5 GB,
  // 2 GB) -> 300 MB
  uint64_t standard_ram_1_5gb = 1536ULL * 1024 * 1024;
  EXPECT_EQ(300ULL * 1024 * 1024,
            CobaltSystemMemoryPressureEvaluator::ResolveProcessMemoryBudget(
                standard_ram_1_5gb));

  uint64_t standard_ram_2gb = 2ULL * 1024 * 1024 * 1024;
  EXPECT_EQ(300ULL * 1024 * 1024,
            CobaltSystemMemoryPressureEvaluator::ResolveProcessMemoryBudget(
                standard_ram_2gb));

  // Tier 4: High-end tier (> 2048 MB physical RAM, e.g. 3 GB, 4 GB) -> 500 MB
  uint64_t high_end_ram = 4ULL * 1024 * 1024 * 1024;
  EXPECT_EQ(500ULL * 1024 * 1024,
            CobaltSystemMemoryPressureEvaluator::ResolveProcessMemoryBudget(
                high_end_ram));
}

TEST_F(CobaltSystemMemoryPressureEvaluatorTest, ParameterSanitization) {
  // Test non-positive poll interval and cooldown:
  // Evaluator should fall back to kDefaultPollInterval and kDefaultCooldown.
  evaluator_.reset();
  auto evaluator = std::make_unique<CobaltSystemMemoryPressureEvaluator>(
      monitor_->CreateVoter(),
      base::BindRepeating(
          &CobaltSystemMemoryPressureEvaluatorTest::GetProcessMemoryInfo,
          base::Unretained(this)),
      base::BindRepeating(
          &CobaltSystemMemoryPressureEvaluatorTest::GetMediaAllowance,
          base::Unretained(this)),
      kTestBudgetBytes,
      /*moderate_process_memory_fraction=*/0.85f,
      /*critical_process_memory_fraction=*/0.95f,
      /*poll_interval=*/base::Seconds(0),
      /*cooldown=*/base::Seconds(-5));

  // With fractions (moderate=0.85, critical=0.95),
  // private memory 180 MB / 200 MB = 90% -> MODERATE.
  SetProcessPrivateMemoryMB(180);
  evaluator->CheckMemoryPressure();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
            evaluator->current_vote());

#if DCHECK_IS_ON()
  // Inverted fractions (moderate > critical) violate invariants and trigger a
  // DCHECK failure.
  EXPECT_DCHECK_DEATH(std::make_unique<CobaltSystemMemoryPressureEvaluator>(
      monitor_->CreateVoter(),
      base::BindRepeating(
          &CobaltSystemMemoryPressureEvaluatorTest::GetProcessMemoryInfo,
          base::Unretained(this)),
      base::BindRepeating(
          &CobaltSystemMemoryPressureEvaluatorTest::GetMediaAllowance,
          base::Unretained(this)),
      kTestBudgetBytes,
      /*moderate_process_memory_fraction=*/0.95f,
      /*critical_process_memory_fraction=*/0.85f,
      /*poll_interval=*/base::Seconds(10),
      /*cooldown=*/base::Seconds(60)));
#endif  // DCHECK_IS_ON()
}

}  // namespace
}  // namespace memory
}  // namespace cobalt

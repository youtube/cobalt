// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"

#include <optional>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory_pressure {

TEST(MultiSourceMemoryPressureMonitorTest, NoEvaluatorUponConstruction) {
  MultiSourceMemoryPressureMonitor monitor;
  EXPECT_FALSE(monitor.system_evaluator_for_testing());
}

TEST(MultiSourceMemoryPressureMonitorTest, RunDispatchCallback) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  MultiSourceMemoryPressureMonitor monitor;
  bool callback_called = false;
  monitor.SetDispatchCallbackForTesting(base::BindLambdaForTesting(
      [&](base::MemoryPressureListener::MemoryPressureLevel) {
        callback_called = true;
      }));
  monitor.MaybeStartPlatformVoter();
  auto* const aggregator = monitor.aggregator_for_testing();

  aggregator->OnVoteForTesting(
      std::nullopt,
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);
  aggregator->NotifyListenersForTesting();
  EXPECT_TRUE(callback_called);

  // Clear vote so aggregator's destructor doesn't think there are loose voters.
  aggregator->OnVoteForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE,
      std::nullopt);
}

#if BUILDFLAG(IS_COBALT)
TEST(MultiSourceMemoryPressureMonitorTest,
     CobaltCooldownAndImmediateEscalation) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  MultiSourceMemoryPressureMonitor monitor;
  std::vector<base::MemoryPressureListener::MemoryPressureLevel> dispatches;
  monitor.SetDispatchCallbackForTesting(base::BindLambdaForTesting(
      [&](base::MemoryPressureListener::MemoryPressureLevel level) {
        dispatches.push_back(level);
      }));

  auto mock_voter = monitor.CreateVoter();

  // Initial MODERATE signal dispatches immediately.
  mock_voter->SetVote(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE, true);
  ASSERT_EQ(dispatches.size(), 1u);
  EXPECT_EQ(dispatches[0],
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Duplicate signal arrives 1s later, suppressed by cooldown.
  task_environment.FastForwardBy(base::Seconds(1));
  mock_voter->SetVote(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE, true);
  EXPECT_EQ(dispatches.size(), 1u);
  EXPECT_EQ(dispatches[0],
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE);

  // Severity increases to CRITICAL 1s later, bypass the cooldown immediately.
  task_environment.FastForwardBy(base::Seconds(1));
  mock_voter->SetVote(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL, true);
  ASSERT_EQ(dispatches.size(), 2u);
  EXPECT_EQ(dispatches[1],
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);

  // Another CRITICAL arrives 2s later, suppressed by cooldown.
  task_environment.FastForwardBy(base::Seconds(2));
  mock_voter->SetVote(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL, true);
  EXPECT_EQ(dispatches.size(), 2u);

  // After cooldown expires (>= 60s), re-evaluation dispatches again.
  task_environment.FastForwardBy(base::Seconds(61));
  mock_voter->SetVote(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL, true);
  ASSERT_EQ(dispatches.size(), 3u);
  EXPECT_EQ(dispatches[2],
            base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL);
}
#endif

}  // namespace memory_pressure

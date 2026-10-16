// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_usage_monitor.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

#include "build/build_config.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_COBALT)
#include "base/test/scoped_feature_list.h"
#include "third_party/blink/public/common/features.h"
#endif

namespace blink {

class CountingObserver : public MemoryUsageMonitor::Observer {
 public:
  void OnMemoryPing(MemoryUsage) override { ++count_; }
  int count() const { return count_; }

 private:
  int count_ = 0;
};

class MemoryUsageMonitorTest : public testing::Test {
 public:
  MemoryUsageMonitorTest() = default;

  void SetUp() override {
    monitor_ = std::make_unique<MemoryUsageMonitor>();
    MemoryUsageMonitor::SetInstanceForTesting(monitor_.get());
  }

  void TearDown() override {
    MemoryUsageMonitor::SetInstanceForTesting(nullptr);
    monitor_.reset();
  }

 private:
  test::TaskEnvironment task_environment_;
  std::unique_ptr<MemoryUsageMonitor> monitor_;
};

TEST_F(MemoryUsageMonitorTest, StartStopMonitor) {
  std::unique_ptr<CountingObserver> observer =
      std::make_unique<CountingObserver>();
  EXPECT_FALSE(MemoryUsageMonitor::Instance().TimerIsActive());
  MemoryUsageMonitor::Instance().AddObserver(observer.get());

  EXPECT_TRUE(MemoryUsageMonitor::Instance().TimerIsActive());
  EXPECT_EQ(0, observer->count());

  test::RunDelayedTasks(base::Seconds(1));
  EXPECT_EQ(1, observer->count());

  test::RunDelayedTasks(base::Seconds(1));
  EXPECT_EQ(2, observer->count());
  MemoryUsageMonitor::Instance().RemoveObserver(observer.get());

  test::RunDelayedTasks(base::Seconds(1));
  EXPECT_EQ(2, observer->count());
  EXPECT_FALSE(MemoryUsageMonitor::Instance().TimerIsActive());
}

class OneShotObserver : public CountingObserver {
 public:
  void OnMemoryPing(MemoryUsage usage) override {
    MemoryUsageMonitor::Instance().RemoveObserver(this);
    CountingObserver::OnMemoryPing(usage);
  }
};

TEST_F(MemoryUsageMonitorTest, RemoveObserverFromNotification) {
  std::unique_ptr<OneShotObserver> observer1 =
      std::make_unique<OneShotObserver>();
  std::unique_ptr<CountingObserver> observer2 =
      std::make_unique<CountingObserver>();
  MemoryUsageMonitor::Instance().AddObserver(observer1.get());
  MemoryUsageMonitor::Instance().AddObserver(observer2.get());
  EXPECT_EQ(0, observer1->count());
  EXPECT_EQ(0, observer2->count());
  test::RunDelayedTasks(base::Seconds(1));
  EXPECT_EQ(1, observer1->count());
  EXPECT_EQ(1, observer2->count());
  test::RunDelayedTasks(base::Seconds(1));
  EXPECT_EQ(1, observer1->count());
  EXPECT_EQ(2, observer2->count());
}

#if BUILDFLAG(IS_COBALT)
TEST_F(MemoryUsageMonitorTest, CustomPollingIntervalOverride) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      blink::features::kMemoryUsageMonitorConfigurable,
      {{"polling_interval_ms", "250"}});

  std::unique_ptr<CountingObserver> observer =
      std::make_unique<CountingObserver>();
  MemoryUsageMonitor::Instance().AddObserver(observer.get());

  EXPECT_TRUE(MemoryUsageMonitor::Instance().TimerIsActive());
  EXPECT_EQ(0, observer->count());

  // Fast forward by exactly 250 milliseconds.
  test::RunDelayedTasks(base::Milliseconds(250));
  EXPECT_EQ(1, observer->count());

  // Fast forward by another 750 milliseconds (filling up exactly 1 second)
  test::RunDelayedTasks(base::Milliseconds(750));
  EXPECT_EQ(4, observer->count());

  MemoryUsageMonitor::Instance().RemoveObserver(observer.get());
}
#endif

}  // namespace blink

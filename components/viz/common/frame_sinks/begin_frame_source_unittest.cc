// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/frame_sinks/begin_frame_source.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/test_mock_time_task_runner.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "components/viz/test/begin_frame_source_test.h"
#include "components/viz/test/fake_delay_based_time_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;
using testing::_;

namespace viz {
namespace {

// Returns a fake TimeTicks based on the given microsecond offset.
base::TimeTicks TicksFromMicroseconds(int64_t micros) {
  return base::TimeTicks() + base::Microseconds(micros);
}

// BeginFrameSource testing ----------------------------------------------------
TEST(BeginFrameSourceTest, SourceIdsAreUnique) {
  StubBeginFrameSource source1;
  StubBeginFrameSource source2;
  StubBeginFrameSource source3;
  EXPECT_NE(source1.source_id(), source2.source_id());
  EXPECT_NE(source1.source_id(), source3.source_id());
  EXPECT_NE(source2.source_id(), source3.source_id());
}

class TestTaskRunner : public base::TestMockTimeTaskRunner {
 public:
  TestTaskRunner()
      : base::TestMockTimeTaskRunner(
            base::TestMockTimeTaskRunner::Type::kStandalone) {
    AdvanceMockTickClock(base::Microseconds(1000));
  }

  TestTaskRunner(const TestTaskRunner&) = delete;
  TestTaskRunner& operator=(const TestTaskRunner&) = delete;

  void FastForwardTo(base::TimeTicks end_time) {
    base::TimeDelta offset = end_time - NowTicks();
    DCHECK_GE(offset, base::TimeDelta());
    FastForwardBy(offset);
  }

 private:
  ~TestTaskRunner() override = default;  // Ref-counted.
};

// BackToBackBeginFrameSource testing
// ------------------------------------------
class BackToBackBeginFrameSourceTest : public ::testing::Test {
 protected:
  static const int64_t kDeadline;
  static const int64_t kInterval;

  void SetUp() override {
    task_runner_ = base::MakeRefCounted<TestTaskRunner>();
    std::unique_ptr<FakeDelayBasedTimeSource> time_source =
        std::make_unique<FakeDelayBasedTimeSource>(
            task_runner_->GetMockTickClock(), task_runner_.get());

    delay_based_time_source_ = time_source.get();
    source_ =
        std::make_unique<BackToBackBeginFrameSource>(std::move(time_source));
    obs_ = std::make_unique<::testing::NiceMock<MockBeginFrameObserver>>();
  }

  void TearDown() override { obs_.reset(); }

  scoped_refptr<TestTaskRunner> task_runner_;
  std::unique_ptr<BackToBackBeginFrameSource> source_;
  std::unique_ptr<MockBeginFrameObserver> obs_;
  raw_ptr<FakeDelayBasedTimeSource>
      delay_based_time_source_;  // Owned by |source_|.
};

const int64_t BackToBackBeginFrameSourceTest::kDeadline =
    BeginFrameArgs::DefaultInterval().InMicroseconds();

const int64_t BackToBackBeginFrameSourceTest::kInterval =
    BeginFrameArgs::DefaultInterval().InMicroseconds();

TEST_F(BackToBackBeginFrameSourceTest, AddObserverSendsBeginFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_TRUE(task_runner_->HasPendingTask());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 1100,
                          1100 + kDeadline, kInterval);
  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  source_->DidFinishFrame(obs_.get());
  task_runner_->RunUntilIdle();
}

TEST_F(BackToBackBeginFrameSourceTest,
       RemoveObserverThenDidFinishFrameProducesNoFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  source_->RemoveObserver(obs_.get());
  source_->DidFinishFrame(obs_.get());

  // Verify no BeginFrame is sent to |obs_|. There is a pending task in the
  // task_runner_ as a BeginFrame was posted, but it gets aborted since |obs_|
  // is removed.
  task_runner_->RunUntilIdle();
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(BackToBackBeginFrameSourceTest,
       DidFinishFrameThenRemoveObserverProducesNoFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  source_->DidFinishFrame(obs_.get());
  source_->RemoveObserver(obs_.get());

  // Task gets cancelled so it doesn't count as a pending task.
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(BackToBackBeginFrameSourceTest,
       TogglingObserverThenDidFinishFrameProducesCorrectFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  source_->RemoveObserver(obs_.get());

  task_runner_->AdvanceMockTickClock(base::Microseconds(10));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());

  task_runner_->AdvanceMockTickClock(base::Microseconds(10));
  source_->DidFinishFrame(obs_.get());

  task_runner_->AdvanceMockTickClock(base::Microseconds(10));
  // The begin frame is posted at the time when the observer was added,
  // so it ignores changes to "now" afterward.
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 1110,
                          1110 + kDeadline, kInterval);
  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->RunUntilIdle();
}

TEST_F(BackToBackBeginFrameSourceTest,
       DidFinishFrameThenTogglingObserverProducesCorrectFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  source_->DidFinishFrame(obs_.get());

  task_runner_->AdvanceMockTickClock(base::Microseconds(10));
  source_->RemoveObserver(obs_.get());

  task_runner_->AdvanceMockTickClock(base::Microseconds(10));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());

  task_runner_->AdvanceMockTickClock(base::Microseconds(10));
  // Ticks at the time at which the observer was added, ignoring the
  // last change to "now".
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 1120,
                          1120 + kDeadline, kInterval);
  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->RunUntilIdle();
}

TEST_F(BackToBackBeginFrameSourceTest, DidFinishFrameNoObserver) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  source_->RemoveObserver(obs_.get());
  source_->DidFinishFrame(obs_.get());
  task_runner_->RunUntilIdle();
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(BackToBackBeginFrameSourceTest, DidFinishFrameMultipleCallsIdempotent) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  source_->DidFinishFrame(obs_.get());
  source_->DidFinishFrame(obs_.get());
  source_->DidFinishFrame(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 1100,
                          1100 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  source_->DidFinishFrame(obs_.get());
  source_->DidFinishFrame(obs_.get());
  source_->DidFinishFrame(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 3, 1200,
                          1200 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();
}

TEST_F(BackToBackBeginFrameSourceTest, DelayInPostedTaskProducesCorrectFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  source_->DidFinishFrame(obs_.get());
  task_runner_->AdvanceMockTickClock(base::Microseconds(50));
  // Ticks at the time the last frame finished, so ignores the last change to
  // "now".
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 1100,
                          1100 + kDeadline, kInterval);

  EXPECT_TRUE(task_runner_->HasPendingTask());
  task_runner_->RunUntilIdle();
}

TEST_F(BackToBackBeginFrameSourceTest, MultipleObserversSynchronized) {
  NiceMock<MockBeginFrameObserver> obs1, obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  source_->AddObserver(&obs1);
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  source_->AddObserver(&obs2);

  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 1, 1000, 1000 + kDeadline,
                          kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 1, 1000, 1000 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  source_->DidFinishFrame(&obs1);
  source_->DidFinishFrame(&obs2);
  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 2, 1100, 1100 + kDeadline,
                          kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 2, 1100, 1100 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  source_->DidFinishFrame(&obs1);
  source_->DidFinishFrame(&obs2);
  EXPECT_TRUE(task_runner_->HasPendingTask());
  source_->RemoveObserver(&obs1);
  source_->RemoveObserver(&obs2);
  task_runner_->RunUntilIdle();
}

TEST_F(BackToBackBeginFrameSourceTest, MultipleObserversInterleaved) {
  NiceMock<MockBeginFrameObserver> obs1, obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  source_->AddObserver(&obs1);
  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 1, 1000, 1000 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  source_->AddObserver(&obs2);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 2, 1100, 1100 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  source_->DidFinishFrame(&obs1);
  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 3, 1200, 1200 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  source_->DidFinishFrame(&obs1);
  source_->RemoveObserver(&obs1);
  // Removing all finished observers should disable the time source.
  EXPECT_FALSE(delay_based_time_source_->Active());
  // Finishing the frame for |obs1| posts a begin frame task, which will be
  // aborted since |obs1| is removed. Clear that from the task runner.
  task_runner_->RunUntilIdle();

  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  source_->DidFinishFrame(&obs2);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 4, 1300, 1300 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  source_->DidFinishFrame(&obs2);
  source_->RemoveObserver(&obs2);
}

TEST_F(BackToBackBeginFrameSourceTest, MultipleObserversAtOnce) {
  NiceMock<MockBeginFrameObserver> obs1, obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  source_->AddObserver(&obs1);
  source_->AddObserver(&obs2);
  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 1, 1000, 1000 + kDeadline,
                          kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 1, 1000, 1000 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  // |obs1| finishes first.
  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  source_->DidFinishFrame(&obs1);

  // |obs2| finishes also, before getting to the newly posted begin frame.
  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  source_->DidFinishFrame(&obs2);

  // Because the begin frame source already ticked when |obs1| finished,
  // we see it as the frame time for both observers.
  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 2, 1100, 1100 + kDeadline,
                          kInterval);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 2, 1100, 1100 + kDeadline,
                          kInterval);
  task_runner_->RunUntilIdle();

  source_->DidFinishFrame(&obs1);
  source_->RemoveObserver(&obs1);
  source_->DidFinishFrame(&obs2);
  source_->RemoveObserver(&obs2);
}

// There may not be any observers available when the OnGpuNoLongerBusy()
// function is invoked on the BackToBackBeginFrameSource. For e.g.
// consider the following sequence.
// 1. An observer is added to the source.
// 2. One BeginFrame request is generated by the timer.
// 3. The clock advances and then the DidFrameFinish() callback is invoked.
//    The timer is marked as active.
// 4. Any pending BeginFrame requests are generated and then the observer
//    unsubscribes.
// 5. If the OnGpuNoLongerBusy() notification is invoked after this step
//    before any new observer is added, a DCHECK about pending observers
//    being empty fires which crashes in debug mode and also in release
//    builds with DCHECK on.
//    With the suggested fix of checking in OnTimerTick() whether the timer
//    is active, this DCHECK should not fire.
TEST_F(BackToBackBeginFrameSourceTest, OnGpuNoLongerBusyWithNoObservers) {
  // Initial setup.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  source_->AddObserver(obs_.get());
  EXPECT_TRUE(task_runner_->HasPendingTask());

  // Expectations for two BeginFrames.
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 1, 1000,
                          1000 + kDeadline, kInterval);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 1100,
                          1100 + kDeadline, kInterval);

  // Advance the clock to ensure that the second BeginFrame has a different
  // timestamp
  task_runner_->AdvanceMockTickClock(base::Microseconds(100));
  task_runner_->RunUntilIdle();

  // Activate the timer so the second BeginFrame can be generated.
  source_->DidFinishFrame(obs_.get());
  task_runner_->RunUntilIdle();

  // Observer is unsubscribed from the source here.
  source_->RemoveObserver(obs_.get());

  // DCHECK for pending observers list being empty should not fire.
  source_->OnGpuNoLongerBusy();
}

// DelayBasedBeginFrameSource testing
// ------------------------------------------
class DelayBasedBeginFrameSourceTest : public ::testing::Test {
 public:
  void SetUp() override {
    task_runner_ = base::MakeRefCounted<TestTaskRunner>();
    std::unique_ptr<FakeDelayBasedTimeSource> time_source =
        std::make_unique<FakeDelayBasedTimeSource>(
            task_runner_->GetMockTickClock(), task_runner_.get());

    time_source->SetTimebaseAndInterval(base::TimeTicks(),
                                        base::Microseconds(10000));
    source_ = std::make_unique<DelayBasedBeginFrameSource>(
        std::move(time_source), BeginFrameSource::kNotRestartableId);
    obs_ = std::make_unique<MockBeginFrameObserver>();
  }

  void TearDown() override { obs_.reset(); }

  scoped_refptr<TestTaskRunner> task_runner_;
  std::unique_ptr<DelayBasedBeginFrameSource> source_;
  std::unique_ptr<MockBeginFrameObserver> obs_;
};

TEST_F(DelayBasedBeginFrameSourceTest,
       AddObserverCallsOnBeginFrameWithMissedTick) {
  task_runner_->AdvanceMockTickClock(base::Microseconds(9010));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 10000, 20000,
                                 10000);
  source_->AddObserver(obs_.get());  // Should cause the last tick to be sent
  // No tasks should need to be run for this to occur.
}

TEST_F(DelayBasedBeginFrameSourceTest, AddObserverCallsCausesOnBeginFrame) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 0, 10000,
                                 10000);
  source_->AddObserver(obs_.get());
  EXPECT_EQ(TicksFromMicroseconds(10000),
            task_runner_->NowTicks() + task_runner_->NextPendingTaskDelay());

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 10000, 20000, 10000);
  task_runner_->AdvanceMockTickClock(base::Microseconds(9010));
  task_runner_->RunUntilIdle();
}

TEST_F(DelayBasedBeginFrameSourceTest, BasicOperation) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 0, 10000,
                                 10000);
  source_->AddObserver(obs_.get());
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 3, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 4, 30000, 40000, 10000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(30001));

  source_->RemoveObserver(obs_.get());
  // No new frames....
  task_runner_->FastForwardTo(TicksFromMicroseconds(60000));
}

TEST_F(DelayBasedBeginFrameSourceTest, VSyncChanges) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 0, 10000,
                                 10000);
  source_->AddObserver(obs_.get());

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 3, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 4, 30000, 40000, 10000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(30001));

  // Update the vsync information
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(27500),
                                   base::Microseconds(10001));

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 5, 40000, 47502, 10001);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 6, 47502, 57503, 10001);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 7, 57503, 67504, 10001);
  task_runner_->FastForwardTo(TicksFromMicroseconds(60000));
}

TEST_F(DelayBasedBeginFrameSourceTest, VSyncChangeTimebaseBeforeLastTick) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 0, 10000,
                                 10000);
  source_->AddObserver(obs_.get());

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 3, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 4, 30000, 40000, 10000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(30000));

  // Update the vsync information such that timebase is before last tick time,
  // and next tick happens within less than the new interval of the following
  // tick (i.e. next_tick -> 40000, following_tick -> 41000)
  // Begin frame won't be used at 41000 because this is a double-tick.
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(26000),
                                   base::Microseconds(5000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 5, 40000, 41000, 5000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 7, 46000, 51000, 5000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(46000));

  // Update the vsync information such that timebase is before last tick time,
  // and next tick happens exactly one interval before the following tick
  // tick (i.e. next_tick -> 51000, following_tick -> 60000)
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(42000),
                                   base::Microseconds(9000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 8, 51000, 60000, 9000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 9, 60000, 69000, 9000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 10, 69000, 78000, 9000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(70000));
}

TEST_F(DelayBasedBeginFrameSourceTest, VSyncChangeTimebaseAfterNextTick) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 0, 10000,
                                 10000);
  source_->AddObserver(obs_.get());

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 3, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 4, 30000, 40000, 10000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(30000));

  // Update the vsync information such that timebase is after next tick time,
  // and next tick happens within less than one interval of the new timebase
  // Begin frame won't be used at 41000 because this is a double-tick.
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(41000),
                                   base::Microseconds(5000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 5, 40000, 41000, 5000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 7, 46000, 51000, 5000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(46000));

  // Update the vsync information such that timebase is after next tick time,
  // and next tick happens exactly one interval before the new timebase
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(60000),
                                   base::Microseconds(9000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 8, 51000, 60000, 9000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 9, 60000, 69000, 9000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 10, 69000, 78000, 9000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(70000));

  // Update the vsync information such that timebase is after next tick time,
  // and next tick happens more than one interval before the new timebase
  // Begin frame won't be used at 80000 because this is a double-tick.
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(100000),
                                   base::Microseconds(5000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 11, 78000, 80000, 5000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 13, 85000, 90000, 5000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(85000));
}

TEST_F(DelayBasedBeginFrameSourceTest, VSyncChangeTimebaseBetweenTicks) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 0, 10000,
                                 10000);
  source_->AddObserver(obs_.get());

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 3, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 4, 30000, 40000, 10000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(30000));

  // Update the vsync information such that timebase is between next tick time,
  // and last tick time.
  // Begin frame won't be used at 41000 because this is a double-tick.
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(35000),
                                   base::Microseconds(6000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 5, 40000, 41000, 6000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 7, 47000, 53000, 6000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(47000));

  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(49000),
                                   base::Microseconds(10000));
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 8, 53000, 59000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 9, 59000, 69000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 10, 69000, 79000, 10000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(70000));
}

TEST_F(DelayBasedBeginFrameSourceTest, VSyncSkipped) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, source_->source_id(), 1, 0, 10000,
                                 10000);
  source_->AddObserver(obs_.get());

  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 2, 10000, 20000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 3, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 4, 30000, 40000, 10000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(30000));

  // Advancing tick time without creating begin frames.
  task_runner_->AdvanceMockTickClock(base::Microseconds(40000));
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(40000),
                                   base::Microseconds(11000));
  // By advancing tick time to 40000, we would be skipping sequence_number 5 at
  // 40000 and sequence_number 6 at 51000.
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 7, 62000, 73000, 11000);
  EXPECT_BEGIN_FRAME_USED(*obs_, source_->source_id(), 8, 73000, 84000, 11000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(75000));
}

TEST_F(DelayBasedBeginFrameSourceTest, MultipleObservers) {
  NiceMock<MockBeginFrameObserver> obs1, obs2;

  // Mock tick clock starts off at 1000.
  task_runner_->FastForwardBy(base::Microseconds(9010));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs1, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs1, source_->source_id(), 1, 10000, 20000,
                                 10000);
  source_->AddObserver(&obs1);  // Should cause the last tick to be sent
  // No tasks should need to be run for this to occur.

  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 2, 20000, 30000, 10000);
  task_runner_->FastForwardBy(base::Microseconds(10000));

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs2, false);
  // Sequence number unchanged for missed frame with time of last normal frame.
  EXPECT_BEGIN_FRAME_USED_MISSED(obs2, source_->source_id(), 2, 20000, 30000,
                                 10000);
  source_->AddObserver(&obs2);  // Should cause the last tick to be sent
  // No tasks should need to be run for this to occur.

  EXPECT_BEGIN_FRAME_USED(obs1, source_->source_id(), 3, 30000, 40000, 10000);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 3, 30000, 40000, 10000);
  task_runner_->FastForwardBy(base::Microseconds(10000));

  source_->RemoveObserver(&obs1);

  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 4, 40000, 50000, 10000);
  task_runner_->FastForwardBy(base::Microseconds(10000));

  source_->RemoveObserver(&obs2);
  task_runner_->FastForwardTo(TicksFromMicroseconds(50000));
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(DelayBasedBeginFrameSourceTest, DoubleTick) {
  NiceMock<MockBeginFrameObserver> obs;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_->source_id(), 1, 0, 10000, 10000);
  source_->AddObserver(&obs);

  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(5000),
                                   base::Microseconds(10000));
  task_runner_->AdvanceMockTickClock(base::Microseconds(4000));

  // No begin frame received.
  task_runner_->RunUntilIdle();

  // Begin frame received.
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(10000),
                                   base::Microseconds(10000));
  task_runner_->AdvanceMockTickClock(base::Microseconds(5000));
  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 2, 10000, 20000, 10000);
  task_runner_->RunUntilIdle();
}

TEST_F(DelayBasedBeginFrameSourceTest, DoubleTickMissedFrame) {
  NiceMock<MockBeginFrameObserver> obs;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_->source_id(), 1, 0, 10000, 10000);
  source_->AddObserver(&obs);
  source_->RemoveObserver(&obs);

  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(5000),
                                   base::Microseconds(10000));
  task_runner_->AdvanceMockTickClock(base::Microseconds(4000));

  // No missed frame received.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  // This does not cause a missed BeginFrame because of double ticking
  // prevention. It does not produce a new sequence number.
  source_->AddObserver(&obs);
  source_->RemoveObserver(&obs);

  // Missed frame received.
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(10000),
                                   base::Microseconds(10000));
  task_runner_->AdvanceMockTickClock(base::Microseconds(5000));
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  // Sequence number is incremented again, because sufficient time has passed.
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_->source_id(), 2, 10000, 20000,
                                 10000);
  source_->AddObserver(&obs);
  source_->RemoveObserver(&obs);
}

TEST_F(DelayBasedBeginFrameSourceTest, MultipleArgsInSameInterval) {
  NiceMock<MockBeginFrameObserver> obs;
  NiceMock<MockBeginFrameObserver> obs2;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_->source_id(), 1, 0, 10000, 10000);
  source_->AddObserver(&obs);
  task_runner_->RunUntilIdle();

  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 2, 10000, 20000, 10000);
  task_runner_->AdvanceMockTickClock(base::Microseconds(9000));
  task_runner_->RunUntilIdle();

  // Sequence number should stay the same within same interval.
  EXPECT_BEGIN_FRAME_USED_MISSED(obs2, source_->source_id(), 2, 10000, 20000,
                                 10000);
  source_->AddObserver(&obs2);

  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 3, 20000, 30000, 10000);
  EXPECT_BEGIN_FRAME_USED(obs2, source_->source_id(), 3, 20000, 30000, 10000);
  task_runner_->AdvanceMockTickClock(base::Microseconds(10000));
  task_runner_->RunUntilIdle();
}

TEST_F(DelayBasedBeginFrameSourceTest, ConsecutiveArgsDelayedByMultipleVsyncs) {
  NiceMock<MockBeginFrameObserver> obs;

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_->source_id(), 1, 0, 10000, 10000);
  source_->AddObserver(&obs);
  task_runner_->RunUntilIdle();

  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 2, 10000, 20000, 10000);
  task_runner_->AdvanceMockTickClock(base::Microseconds(9000));
  task_runner_->RunUntilIdle();
  source_->RemoveObserver(&obs);

  // New args created 8 intervals later.
  // Sequence number should increase by this much.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_->source_id(), 10, 90000, 100000,
                                 10000);
  task_runner_->AdvanceMockTickClock(base::Microseconds(80000));
  source_->AddObserver(&obs);
}

TEST_F(DelayBasedBeginFrameSourceTest, WithVrrInterval) {
  NiceMock<MockBeginFrameObserver> obs;
  source_->SetMaxVrrInterval(base::Microseconds(25000));

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(obs, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(obs, source_->source_id(), 1, 0, 25000, 25000);
  source_->AddObserver(&obs);

  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 2, 10000, 35000, 25000);
  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 3, 20000, 45000, 25000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(21000));
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(21000),
                                   base::Microseconds(10000));
  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 4, 30000, 46000, 25000);
  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 5, 31000, 56000, 25000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(32000));
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(32000),
                                   base::Microseconds(10000));
  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 6, 41000, 57000, 25000);
  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 7, 42000, 67000, 25000);
  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 8, 52000, 77000, 25000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(53000));
  source_->OnUpdateVSyncParameters(TicksFromMicroseconds(53000),
                                   base::Microseconds(10000));
  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 9, 62000, 78000, 25000);
  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 10, 63000, 88000, 25000);
  EXPECT_BEGIN_FRAME_USED(obs, source_->source_id(), 11, 73000, 98000, 25000);
  task_runner_->FastForwardTo(TicksFromMicroseconds(73000));
}

// ExternalBeginFrameSource testing
// --------------------------------------------
class MockExternalBeginFrameSourceClient
    : public ExternalBeginFrameSourceClient {
 public:
  MockExternalBeginFrameSourceClient() = default;
  virtual ~MockExternalBeginFrameSourceClient() = default;

  MOCK_METHOD1(OnNeedsBeginFrames, void(bool));
};

class ExternalBeginFrameSourceTest : public ::testing::Test {
 public:
  ExternalBeginFrameSourceTest()
      : client_(std::make_unique<MockExternalBeginFrameSourceClient>()),
        source_(std::make_unique<ExternalBeginFrameSource>(
            client_.get(),
            BeginFrameSource::kNotRestartableId)),
        obs_(std::make_unique<MockBeginFrameObserver>()) {}

  ~ExternalBeginFrameSourceTest() override = default;

  std::unique_ptr<MockExternalBeginFrameSourceClient> client_;
  std::unique_ptr<ExternalBeginFrameSource> source_;
  std::unique_ptr<MockBeginFrameObserver> obs_;
};

TEST_F(ExternalBeginFrameSourceTest, OnAnimateOnlyBeginFrameOptIn) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_CALL((*client_), OnNeedsBeginFrames(true)).Times(1);
  source_->AddObserver(obs_.get());

  // By default, an observer doesn't receive animate_only BeginFrames.
  BeginFrameArgs args = CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 2, TicksFromMicroseconds(10000));
  args.animate_only = true;
  source_->OnBeginFrame(args);

  // When opting in, an observer receives animate_only BeginFrames.
  args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 3,
                                        TicksFromMicroseconds(10001));
  args.animate_only = true;
  EXPECT_CALL(*obs_, WantsAnimateOnlyBeginFrames())
      .WillOnce(::testing::Return(true));
  EXPECT_BEGIN_FRAME_ARGS_USED(*obs_, args);
  source_->OnBeginFrame(args);

  EXPECT_CALL((*client_), OnNeedsBeginFrames(false)).Times(1);
  source_->RemoveObserver(obs_.get());
}

TEST_F(ExternalBeginFrameSourceTest, OnBeginFrameChecksBeginFrameContinuity) {
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_CALL((*client_), OnNeedsBeginFrames(true)).Times(1);
  source_->AddObserver(obs_.get());

  BeginFrameArgs args = CreateBeginFrameArgsForTesting(
      BEGINFRAME_FROM_HERE, 0, 2, TicksFromMicroseconds(10000));
  EXPECT_BEGIN_FRAME_ARGS_USED(*obs_, args);
  source_->OnBeginFrame(args);

  // Providing same args again to OnBeginFrame() should not notify observer.
  source_->OnBeginFrame(args);

  // Providing same args through a different ExternalBeginFrameSource also
  // does not notify observer.
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_CALL((*client_), OnNeedsBeginFrames(true)).Times(1);
  ExternalBeginFrameSource source2(client_.get());
  source2.AddObserver(obs_.get());
  source2.OnBeginFrame(args);

  EXPECT_CALL((*client_), OnNeedsBeginFrames(false)).Times(2);
  source_->RemoveObserver(obs_.get());
  source2.RemoveObserver(obs_.get());
}

TEST_F(ExternalBeginFrameSourceTest, GetMissedBeginFrameArgs) {
  BeginFrameArgs args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0,
                                                       2, 10000, 10100, 100);
  source_->OnBeginFrame(args);

  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_BEGIN_FRAME_USED_MISSED(*obs_, 0, 2, 10000, 10100, 100);
  source_->AddObserver(obs_.get());
  source_->RemoveObserver(obs_.get());

  // Out of order frame_time. This might not be valid but still shouldn't
  // cause a DCHECK in ExternalBeginFrameSource code.
  args = CreateBeginFrameArgsForTesting(BEGINFRAME_FROM_HERE, 0, 2, 9999, 10100,
                                        101);
  source_->OnBeginFrame(args);

  EXPECT_CALL((*client_), OnNeedsBeginFrames(true)).Times(1);
  EXPECT_BEGIN_FRAME_SOURCE_PAUSED(*obs_, false);
  EXPECT_CALL(*obs_, OnBeginFrame(_)).Times(0);
  source_->AddObserver(obs_.get());

  EXPECT_CALL((*client_), OnNeedsBeginFrames(false)).Times(1);
  source_->RemoveObserver(obs_.get());
}

// Tests that an observer which returns true from IsRoot is notified after
// observers which return false.
TEST_F(ExternalBeginFrameSourceTest, RootsNotifiedLast) {
  using ::testing::InSequence;

  NiceMock<MockBeginFrameObserver> obs1, obs2;
  source_->AddObserver(&obs1);
  source_->AddObserver(&obs2);

  {
    BeginFrameArgs args = CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, 0, 1, 10000, 10100, 100);
    // Set obs1 to root, obs2 to child.
    EXPECT_CALL(obs1, IsRoot()).WillRepeatedly(::testing::Return(true));
    EXPECT_CALL(obs2, IsRoot()).WillRepeatedly(::testing::Return(false));
    {
      // Ensure that OnBeginFrame delivers the calls in the right order.
      InSequence s;
      EXPECT_CALL(obs2, OnBeginFrame(args))
          .WillOnce(::testing::SaveArg<0>(&(obs2.last_begin_frame_args)));
      EXPECT_CALL(obs1, OnBeginFrame(args))
          .WillOnce(::testing::SaveArg<0>(&(obs1.last_begin_frame_args)));
      source_->OnBeginFrame(args);
    }
  }

  {
    BeginFrameArgs args = CreateBeginFrameArgsForTesting(
        BEGINFRAME_FROM_HERE, 0, 2, 10001, 10101, 100);
    // Set obs2 to root, obs1 to child.
    EXPECT_CALL(obs1, IsRoot()).WillRepeatedly(::testing::Return(false));
    EXPECT_CALL(obs2, IsRoot()).WillRepeatedly(::testing::Return(true));
    {
      // Ensure that OnBeginFrame delivers the calls in the right order.
      InSequence s;
      EXPECT_CALL(obs1, OnBeginFrame(args))
          .WillOnce(::testing::SaveArg<0>(&(obs1.last_begin_frame_args)));
      EXPECT_CALL(obs2, OnBeginFrame(args))
          .WillOnce(::testing::SaveArg<0>(&(obs2.last_begin_frame_args)));
      source_->OnBeginFrame(args);
    }
  }

  source_->RemoveObserver(&obs1);
  source_->RemoveObserver(&obs2);
}

#if BUILDFLAG(IS_COBALT)
// Test fixture exercising BufferedSwapBeginFrameSource with a mockable clock
// and task runner.
//
// Lifetime and ownership: Instantiated per GoogleTest case; owns
// |BufferedSwapBeginFrameSource| (which owns |FakeDelayBasedTimeSource|) and
// |MockBeginFrameObserver| across SetUp()..TearDown().
// Threading model: Single-threaded on |TestTaskRunner|.
class BufferedSwapBeginFrameSourceTest : public ::testing::Test {
 protected:
  static constexpr int64_t kIntervalUs = 16666;

  void SetUp() override {
    task_runner_ = base::MakeRefCounted<TestTaskRunner>();
    // Advance past 2 * kIntervalUs so [Now() - 2*interval, Now()] starts at a
    // clean positive timestamp.
    task_runner_->AdvanceMockTickClock(base::Microseconds(99000));
    auto time_source = std::make_unique<FakeDelayBasedTimeSource>(
        task_runner_->GetMockTickClock(), task_runner_.get());
    delay_based_time_source_ = time_source.get();
    source_ = std::make_unique<BufferedSwapBeginFrameSource>(
        std::move(time_source), BeginFrameSource::kNotRestartableId);
    source_->OnUpdateVSyncParameters(base::TimeTicks(),
                                     base::Microseconds(kIntervalUs));
    obs_ = std::make_unique<::testing::NiceMock<MockBeginFrameObserver>>();
  }

  void TearDown() override { obs_.reset(); }

  scoped_refptr<TestTaskRunner> task_runner_;
  std::unique_ptr<BufferedSwapBeginFrameSource> source_;
  std::unique_ptr<MockBeginFrameObserver> obs_;
  raw_ptr<FakeDelayBasedTimeSource> delay_based_time_source_;
};

// Covers the TV UI scrolling journey where fast initial frames (6 ms) pre-fill
// the N=2 EGL back-buffer cushion with frame_time <= Now(), and a subsequent
// 24 ms frame spike is absorbed without skipping a BeginFrame or jumping
// requestAnimationFrame step distances.
TEST_F(BufferedSwapBeginFrameSourceTest,
       PreFillsBackBufferQueueAndAbsorbsTransientSlowFrameWithUniformSteps) {
  std::vector<BeginFrameArgs> received;
  ON_CALL(*obs_, OnBeginFrame(_))
      .WillByDefault(::testing::Invoke([&](const BeginFrameArgs& args) {
        obs_->last_begin_frame_args = args;
        received.push_back(args);
      }));

  // At t = 100000 us, waking from idle starts at Now() - 2 * 16666 = 66668 us.
  source_->AddObserver(obs_.get());
  task_runner_->RunUntilIdle();
  ASSERT_EQ(1u, received.size());
  EXPECT_EQ(base::TimeTicks() + base::Microseconds(66668),
            received[0].frame_time);
  EXPECT_LE(received[0].frame_time, task_runner_->NowTicks());
  EXPECT_EQ(task_runner_->NowTicks() + base::Microseconds(kIntervalUs),
            received[0].deadline);

  // Fast 6 ms frames pre-fill the 2 EGL back buffers until steady state locks
  // onto Now().
  for (int i = 0; i < 3; ++i) {
    task_runner_->AdvanceMockTickClock(base::Microseconds(6000));
    source_->DidFinishFrame(obs_.get());
    task_runner_->RunUntilIdle();
  }
  ASSERT_EQ(4u, received.size());
  // Frame 3 reaches steady state at frame_time = 116666 us (Now() = 118000 us).
  EXPECT_EQ(base::TimeTicks() + base::Microseconds(116666),
            received[3].frame_time);

  // Simulate blocking eglSwapBuffers aligning Frame 3 completion to 133332 us,
  // so Frame 4 starts right at steady state (Now() == frame_time == 133332 us).
  task_runner_->AdvanceMockTickClock(base::Microseconds(15332));
  source_->DidFinishFrame(obs_.get());
  task_runner_->RunUntilIdle();
  ASSERT_EQ(5u, received.size());
  EXPECT_EQ(base::TimeTicks() + base::Microseconds(133332),
            received[4].frame_time);

  // Inject a 24 ms slow frame (133332 -> 157332 us). Because 2 back buffers
  // were queued ahead of scanout, Frame 5 must dispatch immediately with an
  // exact +kIntervalUs step (149998 us), not a +2*kIntervalUs jump.
  task_runner_->AdvanceMockTickClock(base::Microseconds(24000));
  source_->DidFinishFrame(obs_.get());
  task_runner_->RunUntilIdle();
  ASSERT_EQ(6u, received.size());

  for (size_t i = 1; i < received.size(); ++i) {
    EXPECT_EQ(base::Microseconds(kIntervalUs),
              received[i].frame_time - received[i - 1].frame_time);
    EXPECT_LE(received[i].frame_time, task_runner_->NowTicks());
  }
}

// Covers platforms or headless drivers where eglSwapBuffers returns in 1 ms
// without blocking even when the EGL surface buffer queue is full, verifying
// that steady-state frame production never exceeds the target refresh rate.
TEST_F(BufferedSwapBeginFrameSourceTest,
       ThrottlesSteadyStateRateWhenSwapBuffersNeverBlocks) {
  std::vector<BeginFrameArgs> received;
  ON_CALL(*obs_, OnBeginFrame(_))
      .WillByDefault(::testing::Invoke([&](const BeginFrameArgs& args) {
        obs_->last_begin_frame_args = args;
        received.push_back(args);
      }));

  source_->AddObserver(obs_.get());
  task_runner_->RunUntilIdle();
  ASSERT_EQ(1u, received.size());  // frame_time = 66668, now = 100000

  // Two 1 ms swaps complete the initial N=2 pre-fill up to frame_time = 100000.
  for (int i = 0; i < 2; ++i) {
    task_runner_->AdvanceMockTickClock(base::Microseconds(1000));
    source_->DidFinishFrame(obs_.get());
    task_runner_->RunUntilIdle();
  }
  ASSERT_EQ(3u, received.size());
  EXPECT_EQ(base::TimeTicks() + base::Microseconds(100000),
            received[2].frame_time);

  // A 4th frame wants frame_time = 116666 us while Now() is only 103000 us.
  // RunUntilIdle() without advancing the clock must NOT fire early.
  task_runner_->AdvanceMockTickClock(base::Microseconds(1000));
  source_->DidFinishFrame(obs_.get());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(3u, received.size());
  EXPECT_TRUE(task_runner_->HasPendingTask());

  // Advancing to 116666 us dispatches Frame 4 right on schedule.
  task_runner_->FastForwardTo(base::TimeTicks() + base::Microseconds(116666));
  ASSERT_EQ(4u, received.size());
  EXPECT_EQ(base::TimeTicks() + base::Microseconds(116666),
            received[3].frame_time);
}

// Covers genuine UI freezes (> 2 VSyncs) and short idle pauses, verifying that
// frame_time advances by missed VSync multiples when the buffer queue drains
// while remaining strictly monotonic across rapid RemoveObserver/AddObserver.
TEST_F(BufferedSwapBeginFrameSourceTest,
       AdvancesByMissedIntervalsAfterGenuineBufferQueueUnderrunOrIdlePause) {
  std::vector<BeginFrameArgs> received;
  ON_CALL(*obs_, OnBeginFrame(_))
      .WillByDefault(::testing::Invoke([&](const BeginFrameArgs& args) {
        obs_->last_begin_frame_args = args;
        received.push_back(args);
      }));

  source_->AddObserver(obs_.get());
  task_runner_->RunUntilIdle();
  for (int i = 0; i < 2; ++i) {
    task_runner_->AdvanceMockTickClock(base::Microseconds(1000));
    source_->DidFinishFrame(obs_.get());
    task_runner_->RunUntilIdle();
  }
  ASSERT_EQ(3u, received.size());
  // Now at 102000 us, last_frame_time = 100000 us. Simulate a 60 ms freeze
  // (102000 -> 162000 us), which drains the 2-buffer cushion by 1 VSync.
  task_runner_->AdvanceMockTickClock(base::Microseconds(60000));
  source_->DidFinishFrame(obs_.get());
  task_runner_->RunUntilIdle();
  ASSERT_EQ(4u, received.size());
  // Skips 1 dropped VSync (116666 us) and lands at 133332 us inside
  // [162000 - 33332, 162000] so the 2-buffer cushion can refill.
  EXPECT_EQ(base::TimeTicks() + base::Microseconds(133332),
            received[3].frame_time);

  // Rapidly toggling RemoveObserver / AddObserver 2 ms later must preserve
  // monotonicity (>= 133332 us) rather than rewinding by 2 * kIntervalUs.
  source_->RemoveObserver(obs_.get());
  task_runner_->AdvanceMockTickClock(base::Microseconds(2000));
  source_->AddObserver(obs_.get());
  task_runner_->RunUntilIdle();
  ASSERT_EQ(5u, received.size());
  EXPECT_GT(received[4].frame_time, received[3].frame_time);
}

// Covers multi-observer coordination where DisplayScheduler subscribes mid-frame
// while the renderer is still preparing a 24 ms frame, verifying MISSED join
// delivery with an unexpired deadline and barrier synchronization.
TEST_F(BufferedSwapBeginFrameSourceTest,
       SynchronizesMultipleObserversAndDeliversMissedArgsToLateJoiner) {
  NiceMock<MockBeginFrameObserver> renderer_obs;
  NiceMock<MockBeginFrameObserver> display_obs;
  std::vector<BeginFrameArgs> renderer_args;
  std::vector<BeginFrameArgs> display_args;
  ON_CALL(renderer_obs, OnBeginFrame(_))
      .WillByDefault(::testing::Invoke([&](const BeginFrameArgs& args) {
        renderer_obs.last_begin_frame_args = args;
        renderer_args.push_back(args);
      }));
  ON_CALL(display_obs, OnBeginFrame(_))
      .WillByDefault(::testing::Invoke([&](const BeginFrameArgs& args) {
        display_obs.last_begin_frame_args = args;
        display_args.push_back(args);
      }));

  source_->AddObserver(&renderer_obs);
  task_runner_->RunUntilIdle();
  ASSERT_EQ(1u, renderer_args.size());

  // 20 ms later (renderer still busy on Frame 1), DisplayScheduler subscribes.
  task_runner_->AdvanceMockTickClock(base::Microseconds(20000));
  source_->AddObserver(&display_obs);
  ASSERT_EQ(1u, display_args.size());
  EXPECT_EQ(BeginFrameArgs::MISSED, display_args[0].type);
  EXPECT_EQ(renderer_args[0].frame_time, display_args[0].frame_time);
  EXPECT_GT(display_args[0].deadline, task_runner_->NowTicks());

  // DisplayScheduler finishes first; Frame 2 must wait until renderer finishes.
  source_->DidFinishFrame(&display_obs);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(1u, renderer_args.size());

  task_runner_->AdvanceMockTickClock(base::Microseconds(4000));
  source_->DidFinishFrame(&renderer_obs);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(2u, renderer_args.size());
  EXPECT_EQ(2u, display_args.size());

  source_->RemoveObserver(&display_obs);
  source_->RemoveObserver(&renderer_obs);
}

// Covers 50 Hz TV displays (20 ms VSync interval), verifying that once
// OnUpdateVSyncParameters delivers a 20 ms interval, animation frame_time steps
// switch cleanly to 20 ms increments.
TEST_F(BufferedSwapBeginFrameSourceTest,
       AdaptsStepIntervalWhenDisplayRefreshRateUpdatesTo50Hz) {
  std::vector<BeginFrameArgs> received;
  ON_CALL(*obs_, OnBeginFrame(_))
      .WillByDefault(::testing::Invoke([&](const BeginFrameArgs& args) {
        obs_->last_begin_frame_args = args;
        received.push_back(args);
      }));

  source_->OnUpdateVSyncParameters(base::TimeTicks(), base::Milliseconds(20));
  source_->AddObserver(obs_.get());
  task_runner_->RunUntilIdle();
  task_runner_->AdvanceMockTickClock(base::Milliseconds(8));
  source_->DidFinishFrame(obs_.get());
  task_runner_->RunUntilIdle();

  ASSERT_EQ(2u, received.size());
  EXPECT_EQ(base::Milliseconds(20),
            received[1].frame_time - received[0].frame_time);
  EXPECT_EQ(base::Milliseconds(20), received[1].interval);
}

// Covers browser startup where multiple CompositorFrameSinkSupport / Display
// observers subscribe while a BeginFrame tick timer is already armed, and one
// observer unsubscribes synchronously inside OnBeginFrame() after delivering
// presentation timing.
TEST_F(BufferedSwapBeginFrameSourceTest,
       HandlesConcurrentObserverRegistrationWhileTimerActiveAndSyncRemoval) {
  NiceMock<MockBeginFrameObserver> transient_obs;
  NiceMock<MockBeginFrameObserver> renderer_obs;
  std::vector<BeginFrameArgs> renderer_args;

  ON_CALL(transient_obs, OnBeginFrame(_))
      .WillByDefault(::testing::Invoke([&](const BeginFrameArgs& args) {
        transient_obs.last_begin_frame_args = args;
        source_->RemoveObserver(&transient_obs);
      }));
  ON_CALL(renderer_obs, OnBeginFrame(_))
      .WillByDefault(::testing::Invoke([&](const BeginFrameArgs& args) {
        renderer_obs.last_begin_frame_args = args;
        renderer_args.push_back(args);
      }));

  // Register two observers back-to-back while the tick timer is already active,
  // and also exercise ScheduleTickAt() directly while running.
  source_->AddObserver(&transient_obs);
  source_->AddObserver(&renderer_obs);
  delay_based_time_source_->ScheduleTickAt(task_runner_->NowTicks() +
                                           base::Milliseconds(1));
  task_runner_->FastForwardBy(base::Milliseconds(1));

  ASSERT_EQ(1u, renderer_args.size());
  task_runner_->AdvanceMockTickClock(base::Milliseconds(6));
  source_->DidFinishFrame(&renderer_obs);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(2u, renderer_args.size());

  source_->RemoveObserver(&renderer_obs);
}
#endif  // BUILDFLAG(IS_COBALT)

}  // namespace
}  // namespace viz

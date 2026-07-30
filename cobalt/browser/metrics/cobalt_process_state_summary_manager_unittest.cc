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

#include "cobalt/browser/metrics/cobalt_process_state_summary_manager.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "cobalt/browser/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

class TestThread : public base::SimpleThread {
 public:
  template <typename Callable>
  explicit TestThread(Callable&& body)
      : base::SimpleThread("CobaltProcessStateSummaryTest"),
        body_(base::BindLambdaForTesting(std::forward<Callable>(body))) {
    Start();
  }

  void Run() override { std::move(body_).Run(); }

 private:
  base::OnceClosure body_;
};

class FakeCobaltProcessStateSummaryManager
    : public CobaltProcessStateSummaryManager {
 public:
  using FlushCallback =
      base::RepeatingCallback<void(base::span<const uint8_t>)>;

  FakeCobaltProcessStateSummaryManager() = default;
  ~FakeCobaltProcessStateSummaryManager() override = default;

  using CobaltProcessStateSummaryManager::GetCurrentDataForTesting;
  using CobaltProcessStateSummaryManager::IsDirtyForTesting;

  void SetFlushCallbackForTesting(FlushCallback callback) {
    base::AutoLock auto_lock(callback_lock_);
    flush_callback_ = std::move(callback);
  }

  void ResetForTesting() {
    CobaltProcessStateSummaryManager::ResetForTesting();
    base::AutoLock auto_lock(callback_lock_);
    flush_callback_.Reset();
  }

 protected:
  void FlushPayloadToPlatform(base::span<const uint8_t> payload) override {
    CobaltProcessStateSummaryManager::FlushPayloadToPlatform(payload);

    // Copy the test callback under `callback_lock_`, then invoke it outside
    // locks while `flush_in_progress_` is still true so concurrency tests can
    // trigger overlapping calls from worker threads without deadlocking.
    FlushCallback flush_cb;
    {
      base::AutoLock auto_lock(callback_lock_);
      flush_cb = flush_callback_;
    }
    if (flush_cb) {
      flush_cb.Run(payload);
    }
  }

 private:
  mutable base::Lock callback_lock_;
  FlushCallback flush_callback_ GUARDED_BY(callback_lock_);
};

class CobaltProcessStateSummaryManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    manager_ = std::make_unique<FakeCobaltProcessStateSummaryManager>();
    CobaltProcessStateSummaryManager::SetInstanceForTesting(manager_.get());
    EXPECT_EQ(CobaltProcessStateSummaryManager::GetInstance(), manager_.get());
  }

  void TearDown() override {
    CobaltProcessStateSummaryManager::SetInstanceForTesting(nullptr);
    manager_.reset();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<FakeCobaltProcessStateSummaryManager> manager_;
};

TEST_F(CobaltProcessStateSummaryManagerTest,
       SerializeAndDeserialize_RoundTrip) {
  ProcessStateSummaryData original;
  original.startup_guard_armed = true;
  original.startup_guard_triggered_kill = false;
  original.startup_milestones =
      (1ULL << 1) | (1ULL << 5) | (1ULL << 37) | (1ULL << 63);
  original.highest_milestone = 63;

  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::Serialize(original);
  ASSERT_EQ(serialized.size(),
            CobaltProcessStateSummaryManager::kSummaryPayloadSize);
  EXPECT_EQ(serialized[0], CobaltProcessStateSummaryManager::kMagicByte);
  EXPECT_EQ(serialized[1], CobaltProcessStateSummaryManager::kVersionByte);
  EXPECT_EQ(serialized[2],
            CobaltProcessStateSummaryManager::kFlagStartupGuardArmed);
  EXPECT_EQ(serialized[10], 0x80);
  EXPECT_EQ(serialized[11], 63);

  auto deserialized = CobaltProcessStateSummaryManager::Deserialize(serialized);
  ASSERT_TRUE(deserialized.has_value());
  EXPECT_EQ(*deserialized, original);
}

TEST_F(CobaltProcessStateSummaryManagerTest,
       SerializeAndDeserialize_TriggeredKill) {
  ProcessStateSummaryData original;
  original.startup_guard_armed = false;
  original.startup_guard_triggered_kill = true;
  original.startup_milestones = (1ULL << 2) | (1ULL << 10);
  original.highest_milestone = 10;

  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::Serialize(original);
  EXPECT_EQ(serialized[2],
            CobaltProcessStateSummaryManager::kFlagStartupGuardTriggeredKill);

  auto deserialized = CobaltProcessStateSummaryManager::Deserialize(serialized);
  ASSERT_TRUE(deserialized.has_value());
  EXPECT_EQ(*deserialized, original);
}

TEST_F(CobaltProcessStateSummaryManagerTest, Deserialize_InvalidMagic) {
  ProcessStateSummaryData data;
  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::Serialize(data);
  serialized[0] = 0x00;  // Corrupt magic

  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::Deserialize(serialized).has_value());
}

TEST_F(CobaltProcessStateSummaryManagerTest, Deserialize_InvalidVersion) {
  ProcessStateSummaryData data;
  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::Serialize(data);
  serialized[1] = 0x02;  // Corrupt version

  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::Deserialize(serialized).has_value());
}

TEST_F(CobaltProcessStateSummaryManagerTest, Deserialize_CorruptedChecksum) {
  ProcessStateSummaryData data;
  data.highest_milestone = 15;
  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::Serialize(data);

  // Mutate one byte in data payload
  serialized[11] = 16;

  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::Deserialize(serialized).has_value());
}

TEST_F(CobaltProcessStateSummaryManagerTest, Deserialize_TruncatedData) {
  ProcessStateSummaryData data;
  std::vector<uint8_t> serialized =
      CobaltProcessStateSummaryManager::Serialize(data);
  serialized.resize(15);  // Truncated to 15 bytes

  EXPECT_FALSE(
      CobaltProcessStateSummaryManager::Deserialize(serialized).has_value());
}

TEST_F(CobaltProcessStateSummaryManagerTest,
       SetStartupMilestone_UpdatesBitmaskAndHighest) {
  manager_->SetStartupMilestone(1);
  manager_->SetStartupMilestone(5);
  manager_->SetStartupMilestone(37);

  ProcessStateSummaryData current = manager_->GetCurrentDataForTesting();
  EXPECT_EQ(current.startup_milestones,
            (1ULL << 1) | (1ULL << 5) | (1ULL << 37));
  EXPECT_EQ(current.highest_milestone, 37);

  // Duplicate milestone should be idempotent
  manager_->SetStartupMilestone(5);
  current = manager_->GetCurrentDataForTesting();
  EXPECT_EQ(current.startup_milestones,
            (1ULL << 1) | (1ULL << 5) | (1ULL << 37));
  EXPECT_EQ(current.highest_milestone, 37);

  // Lower milestone should not decrease highest_milestone
  manager_->SetStartupMilestone(10);
  current = manager_->GetCurrentDataForTesting();
  EXPECT_EQ(current.startup_milestones,
            (1ULL << 1) | (1ULL << 5) | (1ULL << 10) | (1ULL << 37));
  EXPECT_EQ(current.highest_milestone, 37);
}

TEST_F(CobaltProcessStateSummaryManagerTest,
       SetStartupMilestone_IgnoresOutOfBoundsAndHandlesBoundary) {
  manager_->SetStartupMilestone(-1);
  manager_->SetStartupMilestone(0);
  manager_->SetStartupMilestone(64);
  EXPECT_EQ(manager_->GetCurrentDataForTesting().startup_milestones, 0ULL);
  EXPECT_EQ(manager_->GetCurrentDataForTesting().highest_milestone, 0);
  EXPECT_FALSE(manager_->IsDirtyForTesting());

  // Upper valid boundary (63) should be recorded and flushed immediately.
  manager_->SetStartupMilestone(63);
  EXPECT_EQ(manager_->GetCurrentDataForTesting().startup_milestones,
            1ULL << 63);
  EXPECT_EQ(manager_->GetCurrentDataForTesting().highest_milestone, 63);
  EXPECT_FALSE(manager_->IsDirtyForTesting());

  // Redundant milestone 63 within cooldown should be a no-op and not set dirty.
  manager_->SetStartupMilestone(63);
  EXPECT_FALSE(manager_->IsDirtyForTesting());
}

TEST_F(CobaltProcessStateSummaryManagerTest,
       SetStartupGuardArmed_FlushesImmediately) {
  manager_->SetStartupGuardArmed(true);
  EXPECT_TRUE(manager_->GetCurrentDataForTesting().startup_guard_armed);
  EXPECT_FALSE(manager_->IsDirtyForTesting());

  // Subsequent milestone within 15s cooldown is coalesced (dirty = true).
  manager_->SetStartupMilestone(14);
  EXPECT_TRUE(manager_->IsDirtyForTesting());

  // Redundant SetStartupGuardArmed(true) takes early return and leaves dirty.
  manager_->SetStartupGuardArmed(true);
  EXPECT_TRUE(manager_->IsDirtyForTesting());

  // Disarming StartupGuard flushes pending milestone state immediately.
  manager_->SetStartupGuardArmed(false);
  EXPECT_FALSE(manager_->GetCurrentDataForTesting().startup_guard_armed);
  EXPECT_FALSE(manager_->IsDirtyForTesting());
}

TEST_F(CobaltProcessStateSummaryManagerTest,
       RateLimitingCooldown_FlushesAfterMinInterval) {
  // First milestone flushes immediately at t=0s.
  manager_->SetStartupMilestone(1);
  EXPECT_FALSE(manager_->IsDirtyForTesting());

  // Subsequent milestones within the 15s default min_flush_interval are
  // coalesced and scheduled for a trailing flush when the cooldown expires.
  task_environment_.FastForwardBy(base::Seconds(5));
  manager_->SetStartupMilestone(5);
  EXPECT_TRUE(manager_->IsDirtyForTesting());

  task_environment_.FastForwardBy(base::Seconds(9));
  manager_->SetStartupMilestone(10);
  EXPECT_TRUE(manager_->IsDirtyForTesting());

  // Advance 1s more (total 15s >= default min_flush_interval): trailing timer
  // flushes coalesced milestones 5 and 10 automatically.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(manager_->IsDirtyForTesting());
}

TEST_F(CobaltProcessStateSummaryManagerTest,
       RateLimitingCooldown_RespectsFinchFeatureParam) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltProcessStateSummary, {{"min_flush_interval", "5s"}});

  manager_->SetStartupMilestone(1);
  EXPECT_FALSE(manager_->IsDirtyForTesting());

  // Advance 4s (< 5s overridden min_flush_interval): coalesced.
  task_environment_.FastForwardBy(base::Seconds(4));
  manager_->SetStartupMilestone(5);
  manager_->SetStartupMilestone(10);
  EXPECT_TRUE(manager_->IsDirtyForTesting());

  // Advance 1s more (total 5s >= overridden min_flush_interval): trailing
  // timer flushes coalesced milestones 5 and 10 automatically.
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(manager_->IsDirtyForTesting());
}

TEST_F(CobaltProcessStateSummaryManagerTest,
       FlushSummaryImmediate_ClearsDirtyStateWithinCooldown) {
  base::HistogramTester histogram_tester;

  // First milestone flushes immediately.
  manager_->SetStartupMilestone(1);
  EXPECT_FALSE(manager_->IsDirtyForTesting());
  histogram_tester.ExpectTotalCount(
      "Cobalt.Stability.ProcessStateSummary.FlushDurationUs", 1);

  // Second milestone within 15s cooldown remains dirty until flushed.
  manager_->SetStartupMilestone(5);
  EXPECT_TRUE(manager_->IsDirtyForTesting());
  histogram_tester.ExpectTotalCount(
      "Cobalt.Stability.ProcessStateSummary.FlushDurationUs", 1);

  manager_->FlushSummaryImmediate();
  EXPECT_FALSE(manager_->IsDirtyForTesting());
  histogram_tester.ExpectTotalCount(
      "Cobalt.Stability.ProcessStateSummary.FlushDurationUs", 2);

  int exit_reason = -1;
  (void)manager_->GetPriorSessionSummary(&exit_reason);
  histogram_tester.ExpectTotalCount(
      "Cobalt.Stability.ProcessStateSummary.PriorSessionReadDurationUs", 1);
}

TEST_F(CobaltProcessStateSummaryManagerTest, DisabledFeature_ActsAsKillSwitch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kCobaltProcessStateSummary);

  manager_->SetStartupGuardArmed(true);
  manager_->SetStartupMilestone(14);
  manager_->FlushSummaryImmediate();

  ProcessStateSummaryData current = manager_->GetCurrentDataForTesting();
  EXPECT_FALSE(current.startup_guard_armed);
  EXPECT_EQ(current.startup_milestones, 0ULL);
  EXPECT_EQ(current.highest_milestone, 0);
  EXPECT_FALSE(manager_->IsDirtyForTesting());

  int exit_reason = 42;
  EXPECT_FALSE(manager_->GetPriorSessionSummary(&exit_reason).has_value());
  EXPECT_EQ(exit_reason, -1);
}

TEST_F(CobaltProcessStateSummaryManagerTest,
       ConcurrentMultiThreadedUpdatesAndFlushes_NeverDeadlocksOrLosesBits) {
  constexpr int kNumThreads = 8;
  constexpr int kIterationsPerThread = 250;

  base::WaitableEvent start_gate(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  std::vector<std::unique_ptr<TestThread>> threads;
  threads.reserve(kNumThreads);
  for (int t = 0; t < kNumThreads; ++t) {
    threads.push_back(std::make_unique<TestThread>([this, t, &start_gate]() {
      start_gate.Wait();
      for (int i = 0; i < kIterationsPerThread; ++i) {
        const int milestone = ((t * kIterationsPerThread + i) % 63) + 1;
        manager_->SetStartupMilestone(milestone);
        if ((i % 7) == 0) {
          manager_->SetStartupGuardArmed((i % 2) == 0);
        }
        if ((i % 11) == 0) {
          manager_->FlushSummaryImmediate();
        }
        if ((i % 13) == 0) {
          (void)manager_->GetCurrentDataForTesting();
          (void)manager_->IsDirtyForTesting();
        }
      }
    }));
  }

  // Release all worker threads simultaneously to maximize lock and
  // single-flight flush contention.
  start_gate.Signal();

  for (auto& thread : threads) {
    thread->Join();
  }

  manager_->SetStartupGuardArmed(false);
  manager_->FlushSummaryImmediate();

  const ProcessStateSummaryData final_data =
      manager_->GetCurrentDataForTesting();
  // All milestone bits 1..63 must be set (bit 0 is unused).
  EXPECT_EQ(final_data.startup_milestones, ~1ULL);
  EXPECT_EQ(final_data.highest_milestone, 63);
  EXPECT_FALSE(final_data.startup_guard_armed);
  EXPECT_FALSE(manager_->IsDirtyForTesting());
}

TEST_F(CobaltProcessStateSummaryManagerTest,
       DelayedMidFlushAndReentrantUpdates_NonBlockingAndDrainsInStrictOrder) {
  base::Lock payloads_lock;
  std::vector<ProcessStateSummaryData> flushed_states;

  base::WaitableEvent entered_first_flush(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent release_first_flush(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent thread_b_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent thread_c_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent flusher_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  std::atomic<int> flush_count{0};

  manager_->SetFlushCallbackForTesting(base::BindRepeating(
      [](CobaltProcessStateSummaryManager* mgr, base::Lock* lock,
         std::vector<ProcessStateSummaryData>* states, std::atomic<int>* count,
         base::WaitableEvent* entered, base::WaitableEvent* release,
         base::span<const uint8_t> bytes) {
        auto parsed = CobaltProcessStateSummaryManager::Deserialize(bytes);
        EXPECT_TRUE(parsed.has_value());
        if (parsed.has_value()) {
          base::AutoLock auto_lock(*lock);
          states->push_back(*parsed);
        }
        if (count->fetch_add(1) == 0) {
          // 1. Same-thread re-entrant update while flush_in_progress_ is true.
          // If lock_ were held across the flush, this would deadlock
          // immediately.
          mgr->SetStartupMilestone(2);

          // 2. Signal that Thread A is inside the first flush, then wait to
          // simulate a slow JNI/Binder IPC call.
          entered->Signal();
          EXPECT_TRUE(release->TimedWait(base::Seconds(5)));
        }
      },
      manager_.get(), &payloads_lock, &flushed_states, &flush_count,
      &entered_first_flush, &release_first_flush));

  // Thread A: triggers the initial flush and blocks inside the delayed flush
  // callback.
  TestThread thread_a([this, &flusher_done]() {
    manager_->SetStartupMilestone(1);
    flusher_done.Signal();
  });

  // Wait until Thread A is inside the first flush callback.
  ASSERT_TRUE(entered_first_flush.TimedWait(base::Seconds(5)));
  EXPECT_FALSE(flusher_done.IsSignaled());

  // Thread C waits on Thread B to finish first (verifying ordered wait).
  TestThread thread_c([this, &thread_b_done, &thread_c_done]() {
    EXPECT_TRUE(thread_b_done.TimedWait(base::Seconds(5)));
    manager_->SetStartupGuardArmed(true);
    manager_->FlushSummaryImmediate();
    thread_c_done.Signal();
  });

  // Before Thread B runs, Thread C must still be waiting.
  EXPECT_FALSE(thread_c_done.IsSignaled());
  EXPECT_FALSE(flusher_done.IsSignaled());

  // Thread B updates milestone 3 while Thread A is still blocked mid-flush.
  TestThread thread_b([this, &thread_b_done]() {
    manager_->SetStartupMilestone(3);
    thread_b_done.Signal();
  });

  // Both Thread B and Thread C must complete WITHOUT blocking on Thread A's
  // in-flight flush.
  ASSERT_TRUE(thread_b_done.TimedWait(base::Seconds(5)));
  ASSERT_TRUE(thread_c_done.TimedWait(base::Seconds(5)));
  // Thread A must still be waiting for release_first_flush.
  EXPECT_FALSE(flusher_done.IsSignaled());

  // Now release Thread A from its first flush so it drains the pending updates.
  release_first_flush.Signal();
  ASSERT_TRUE(flusher_done.TimedWait(base::Seconds(5)));

  thread_a.Join();
  thread_b.Join();
  thread_c.Join();

  base::AutoLock auto_lock(payloads_lock);
  ASSERT_EQ(flushed_states.size(), 2u);
  // First flush only had milestone 1.
  EXPECT_EQ(flushed_states[0].startup_milestones, 1ULL << 1);
  EXPECT_EQ(flushed_states[0].highest_milestone, 1);
  EXPECT_FALSE(flushed_states[0].startup_guard_armed);

  // Second (drained) flush coalesced re-entrant milestone 2, Thread B's
  // milestone 3, and Thread C's startup_guard_armed = true in strict order.
  EXPECT_EQ(flushed_states[1].startup_milestones,
            (1ULL << 1) | (1ULL << 2) | (1ULL << 3));
  EXPECT_EQ(flushed_states[1].highest_milestone, 3);
  EXPECT_TRUE(flushed_states[1].startup_guard_armed);
  EXPECT_FALSE(manager_->IsDirtyForTesting());
}

TEST_F(CobaltProcessStateSummaryManagerTest,
       AllOperationPermutationsMidFlush_NeverDeadlocksOrReorders) {
  // 4 distinct operations executed across threads in all 4! = 24 permutations
  // while an initial flush is actively in flight:
  //   0: SetStartupMilestone(5)
  //   1: SetStartupMilestone(42)
  //   2: SetStartupGuardArmed(true)
  //   3: SetStartupGuardArmed(false)
  std::vector<int> perm = {0, 1, 2, 3};

  do {
    manager_->ResetForTesting();

    base::Lock payloads_lock;
    std::vector<ProcessStateSummaryData> flushed_states;
    std::atomic<int> flush_count{0};

    base::WaitableEvent entered_flush(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::WaitableEvent release_flush(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::WaitableEvent flusher_done(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    manager_->SetFlushCallbackForTesting(base::BindRepeating(
        [](base::Lock* lock, std::vector<ProcessStateSummaryData>* states,
           std::atomic<int>* count, base::WaitableEvent* entered,
           base::WaitableEvent* release, base::span<const uint8_t> bytes) {
          auto parsed = CobaltProcessStateSummaryManager::Deserialize(bytes);
          EXPECT_TRUE(parsed.has_value());
          if (parsed.has_value()) {
            base::AutoLock auto_lock(*lock);
            states->push_back(*parsed);
          }
          if (count->fetch_add(1) == 0) {
            entered->Signal();
            EXPECT_TRUE(release->TimedWait(base::Seconds(5)));
          }
        },
        &payloads_lock, &flushed_states, &flush_count, &entered_flush,
        &release_flush));

    // Start flusher thread with milestone 1 and hold it mid-flush.
    TestThread flusher([this, &flusher_done]() {
      manager_->SetStartupMilestone(1);
      flusher_done.Signal();
    });

    ASSERT_TRUE(entered_flush.TimedWait(base::Seconds(5)));

    // Coordinate 4 worker threads to execute perm[0..3] in exact sequence while
    // the flusher thread is paused mid-flush.
    std::vector<std::unique_ptr<base::WaitableEvent>> step_done;
    for (int i = 0; i < 4; ++i) {
      step_done.push_back(std::make_unique<base::WaitableEvent>(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED));
    }

    std::vector<std::unique_ptr<TestThread>> workers;
    workers.reserve(4);
    for (int step = 0; step < 4; ++step) {
      const int op = perm[step];
      workers.push_back(
          std::make_unique<TestThread>([this, step, op, &step_done]() {
            if (step > 0) {
              EXPECT_TRUE(step_done[step - 1]->TimedWait(base::Seconds(5)));
            }
            switch (op) {
              case 0:
                manager_->SetStartupMilestone(5);
                break;
              case 1:
                manager_->SetStartupMilestone(42);
                break;
              case 2:
                manager_->SetStartupGuardArmed(true);
                break;
              case 3:
                manager_->SetStartupGuardArmed(false);
                break;
            }
            step_done[step]->Signal();
          }));
    }

    ASSERT_TRUE(step_done[3]->TimedWait(base::Seconds(5)));
    // Ensure a pending immediate flush is queued even if the final guard op in
    // this permutation was a no-op (e.g., SetStartupGuardArmed(false) when
    // already false).
    manager_->FlushSummaryImmediate();
    EXPECT_FALSE(flusher_done.IsSignaled());

    // Release the mid-flush barrier and wait for the flusher to drain.
    release_flush.Signal();
    ASSERT_TRUE(flusher_done.TimedWait(base::Seconds(5)));

    flusher.Join();
    for (auto& w : workers) {
      w->Join();
    }

    // Verify expected final state for this permutation:
    // Op 2 arms the guard, Op 3 disarms it, so the guard is armed iff Op 2
    // appears after Op 3 in `perm`.
    const auto pos_arm = std::find(perm.begin(), perm.end(), 2);
    const auto pos_disarm = std::find(perm.begin(), perm.end(), 3);
    const bool expected_armed = pos_arm > pos_disarm;
    const uint64_t expected_milestones =
        (1ULL << 1) | (1ULL << 5) | (1ULL << 42);

    base::AutoLock auto_lock(payloads_lock);
    ASSERT_EQ(flushed_states.size(), 2u);
    // Verify monotonic progression across flushes (no stale overwrites).
    EXPECT_EQ(flushed_states[0].startup_milestones &
                  ~flushed_states[1].startup_milestones,
              0ULL);
    EXPECT_LE(flushed_states[0].highest_milestone,
              flushed_states[1].highest_milestone);

    EXPECT_EQ(flushed_states.back().startup_milestones, expected_milestones);
    EXPECT_EQ(flushed_states.back().highest_milestone, 42);
    EXPECT_EQ(flushed_states.back().startup_guard_armed, expected_armed);
    EXPECT_FALSE(manager_->IsDirtyForTesting());
  } while (std::next_permutation(perm.begin(), perm.end()));
}

}  // namespace
}  // namespace cobalt

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

#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "cobalt/browser/metrics/cobalt_process_state_summary_manager.h"
#include "cobalt/testing/browser_tests/browser/test_shell.h"
#include "cobalt/testing/browser_tests/content_browser_test.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace cobalt {
namespace {

class TestCobaltProcessStateSummaryManager
    : public CobaltProcessStateSummaryManager {
 public:
  using FlushCallback =
      base::RepeatingCallback<void(base::span<const uint8_t>)>;

  TestCobaltProcessStateSummaryManager() = default;
  ~TestCobaltProcessStateSummaryManager() override = default;

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

class CobaltProcessStateSummaryBrowserTest
    : public content::ContentBrowserTest {
 public:
  CobaltProcessStateSummaryBrowserTest() = default;
  ~CobaltProcessStateSummaryBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
    test_manager_ = std::make_unique<TestCobaltProcessStateSummaryManager>();
    CobaltProcessStateSummaryManager::SetInstanceForTesting(
        test_manager_.get());
  }

  void TearDownOnMainThread() override {
    CobaltProcessStateSummaryManager::SetInstanceForTesting(nullptr);
    test_manager_.reset();
    content::ContentBrowserTest::TearDownOnMainThread();
  }

 protected:
  TestCobaltProcessStateSummaryManager* test_manager() {
    return test_manager_.get();
  }

 private:
  std::unique_ptr<TestCobaltProcessStateSummaryManager> test_manager_;
};

IN_PROC_BROWSER_TEST_F(
    CobaltProcessStateSummaryBrowserTest,
    ConcurrentCrossThreadUpdatesDuringNavigation_NeverDeadlocks) {
  auto* manager = test_manager();
  ASSERT_EQ(CobaltProcessStateSummaryManager::GetInstance(), manager);

  constexpr int kNumThreadPoolTasks = 6;
  constexpr int kIterationsPerTask = 150;
  // Total async task groups: kNumThreadPoolTasks + 1 IO thread task.
  base::RunLoop wait_for_workers;
  base::RepeatingClosure barrier = base::BarrierClosure(
      kNumThreadPoolTasks + 1, wait_for_workers.QuitClosure());

  // 1. Dispatch concurrent updates on the Browser IO thread.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(
                     [](CobaltProcessStateSummaryManager* mgr,
                        base::RepeatingClosure done_closure) {
                       for (int i = 0; i < kIterationsPerTask; ++i) {
                         const int milestone = (i % 63) + 1;
                         mgr->SetStartupMilestone(milestone);
                         if ((i % 5) == 0) {
                           mgr->SetStartupGuardArmed((i % 2) == 0);
                         }
                         if ((i % 9) == 0) {
                           mgr->FlushSummaryImmediate();
                         }
                       }
                       done_closure.Run();
                     },
                     manager, barrier));

  // 2. Dispatch concurrent updates across multiple ThreadPool worker threads.
  for (int t = 0; t < kNumThreadPoolTasks; ++t) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            [](CobaltProcessStateSummaryManager* mgr, int task_idx,
               base::RepeatingClosure done_closure) {
              for (int i = 0; i < kIterationsPerTask; ++i) {
                const int milestone =
                    ((task_idx * kIterationsPerTask + i) % 63) + 1;
                mgr->SetStartupMilestone(milestone);
                if ((i % 7) == 0) {
                  mgr->SetStartupGuardArmed((i % 2) == 1);
                }
                if ((i % 11) == 0) {
                  mgr->FlushSummaryImmediate();
                }
                if ((i % 17) == 0) {
                  int exit_reason = -1;
                  (void)mgr->GetPriorSessionSummary(&exit_reason);
                }
              }
              done_closure.Run();
            },
            manager, t, barrier));
  }

  // 3. Concurrently navigate WebContents and update milestones on the Browser
  // UI thread while IO and ThreadPool threads are actively flushing.
  GURL url(
      "data:text/html;charset=utf-8,"
      "<html><body><h1>Process State Summary Concurrency</h1></body></html>");
  ASSERT_TRUE(content::NavigateToURL(shell()->web_contents(), url));

  for (int m = 1; m <= 63; ++m) {
    manager->SetStartupMilestone(m);
  }
  manager->FlushSummaryImmediate();

  // Wait for all IO and ThreadPool tasks to complete without deadlock.
  wait_for_workers.Run();

  manager->SetStartupGuardArmed(false);
  manager->FlushSummaryImmediate();

  const ProcessStateSummaryData final_data =
      manager->GetCurrentDataForTesting();
  EXPECT_EQ(final_data.startup_milestones, ~1ULL);
  EXPECT_EQ(final_data.highest_milestone, 63);
  EXPECT_FALSE(final_data.startup_guard_armed);
  EXPECT_FALSE(manager->IsDirtyForTesting());
}

IN_PROC_BROWSER_TEST_F(
    CobaltProcessStateSummaryBrowserTest,
    AllBrowserThreadPermutationsWithDelayedFlush_NeverDeadlocksOrReorders) {
  base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync;
  auto* manager = test_manager();
  ASSERT_EQ(CobaltProcessStateSummaryManager::GetInstance(), manager);

  // Exercise all 3! = 6 permutations of the three browser thread types:
  //   0: Browser UI Thread
  //   1: Browser IO Thread
  //   2: ThreadPool Worker Thread
  // In each permutation:
  //   - perm[0] triggers the initial flush (milestone 10) and sleeps/waits
  //     inside the flush callback while `flush_in_progress_ == true`.
  //   - perm[1] waits for perm[0] to enter the flush, delays briefly, records
  //     milestone 25, and signals `step1_done`.
  //   - perm[2] waits for `step1_done`, records milestone 50 and arms
  //     StartupGuard, signals `step2_done`, and releases perm[0]'s flush.
  std::vector<int> perm = {0, 1, 2};

  do {
    manager->ResetForTesting();

    base::Lock payloads_lock;
    std::vector<ProcessStateSummaryData> flushed_states;
    std::atomic<int> flush_count{0};

    base::WaitableEvent entered_flush(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::WaitableEvent step1_done(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::WaitableEvent step2_done(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::WaitableEvent release_flush(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    base::WaitableEvent flusher_done(
        base::WaitableEvent::ResetPolicy::MANUAL,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    manager->SetFlushCallbackForTesting(base::BindRepeating(
        [](base::Lock* lock, std::vector<ProcessStateSummaryData>* states,
           std::atomic<int>* count, base::WaitableEvent* entered,
           base::WaitableEvent* release, base::span<const uint8_t> bytes) {
          base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync_in_cb;
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

    struct RoleContext {
      CobaltProcessStateSummaryManager* manager;
      base::WaitableEvent* entered_flush;
      base::WaitableEvent* step1_done;
      base::WaitableEvent* step2_done;
      base::WaitableEvent* release_flush;
      base::WaitableEvent* flusher_done;
    } role_ctx{manager,     &entered_flush, &step1_done,
               &step2_done, &release_flush, &flusher_done};

    auto run_role = [](RoleContext* ctx, int role) {
      base::ScopedAllowBaseSyncPrimitivesForTesting allow_sync_in_role;
      if (role == 0) {
        ctx->manager->SetStartupMilestone(10);
        ctx->flusher_done->Signal();
      } else if (role == 1) {
        EXPECT_TRUE(ctx->entered_flush->TimedWait(base::Seconds(5)));
        EXPECT_FALSE(ctx->step2_done->IsSignaled());
        ctx->manager->SetStartupMilestone(25);
        ctx->step1_done->Signal();
      } else {
        EXPECT_TRUE(ctx->step1_done->TimedWait(base::Seconds(5)));
        EXPECT_FALSE(ctx->flusher_done->IsSignaled());
        ctx->manager->SetStartupMilestone(50);
        ctx->manager->SetStartupGuardArmed(true);
        ctx->step2_done->Signal();
        ctx->release_flush->Signal();
      }
    };

    int ui_role = -1;
    for (int role = 0; role < 3; ++role) {
      const int thread_type = perm[role];
      if (thread_type == 0) {
        ui_role = role;
      } else if (thread_type == 1) {
        content::GetIOThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(run_role, &role_ctx, role));
      } else {
        base::ThreadPool::PostTask(FROM_HERE, {base::MayBlock()},
                                   base::BindOnce(run_role, &role_ctx, role));
      }
    }

    // Execute the UI thread's assigned role in this permutation directly on the
    // Browser UI thread.
    ASSERT_NE(ui_role, -1);
    run_role(&role_ctx, ui_role);

    ASSERT_TRUE(step1_done.TimedWait(base::Seconds(5)));
    ASSERT_TRUE(step2_done.TimedWait(base::Seconds(5)));
    ASSERT_TRUE(flusher_done.TimedWait(base::Seconds(5)));

    base::AutoLock auto_lock(payloads_lock);
    ASSERT_EQ(flushed_states.size(), 2u);
    EXPECT_EQ(flushed_states[0].startup_milestones, 1ULL << 10);
    EXPECT_EQ(flushed_states[0].highest_milestone, 10);
    EXPECT_FALSE(flushed_states[0].startup_guard_armed);

    EXPECT_EQ(flushed_states[1].startup_milestones,
              (1ULL << 10) | (1ULL << 25) | (1ULL << 50));
    EXPECT_EQ(flushed_states[1].highest_milestone, 50);
    EXPECT_TRUE(flushed_states[1].startup_guard_armed);
    EXPECT_FALSE(manager->IsDirtyForTesting());
  } while (std::next_permutation(perm.begin(), perm.end()));
}

}  // namespace
}  // namespace cobalt

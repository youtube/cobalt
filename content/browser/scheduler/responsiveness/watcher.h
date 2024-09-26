// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_WATCHER_H_
#define CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_WATCHER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/time/time.h"
#include "content/browser/scheduler/responsiveness/metric_source.h"
#include "content/common/content_export.h"

namespace content {
namespace responsiveness {

class Calculator;

class CONTENT_EXPORT Watcher : public base::RefCounted<Watcher>,
                               public MetricSource::Delegate {
 public:
  Watcher();
  void SetUp();
  void Destroy();

  // Must be invoked once-and-only-once, after SetUp(), the first time
  // MainMessageLoopRun() reaches idle (i.e. done running all tasks queued
  // during startup). This will be used as a signal for the true end of
  // "startup" and the beginning of recording Browser.MainThreadsCongestion.
  void OnFirstIdle();

 protected:
  friend class base::RefCounted<Watcher>;

  // Exposed for tests.
  virtual std::unique_ptr<Calculator> CreateCalculator();
  virtual std::unique_ptr<MetricSource> CreateMetricSource();

  ~Watcher() override;

  // Delegate interface implementation.
  void SetUpOnIOThread() override;
  void TearDownOnUIThread() override;
  void TearDownOnIOThread() override;

  void WillRunTaskOnUIThread(const base::PendingTask* task,
                             bool was_blocked_or_low_priority) override;
  void DidRunTaskOnUIThread(const base::PendingTask* task) override;

  void WillRunTaskOnIOThread(const base::PendingTask* task,
                             bool was_blocked_or_low_priority) override;
  void DidRunTaskOnIOThread(const base::PendingTask* task) override;

  void WillRunEventOnUIThread(const void* opaque_identifier) override;
  void DidRunEventOnUIThread(const void* opaque_identifier) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, TaskForwarding);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, TaskNesting);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, NativeEvents);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, BlockedOrLowPriorityTask);
  FRIEND_TEST_ALL_PREFIXES(ResponsivenessWatcherTest, DelayedTask);

  // Metadata for currently running tasks and events is needed to track whether
  // or not they caused reentrancy.
  struct Metadata {
    explicit Metadata(const void* identifier,
                      bool was_blocked_or_low_priority,
                      base::TimeTicks execution_start_time);

    // An opaque identifier for the task or event.
    //
    // `identifier` is not a raw_ptr<...> for performance reasons (based on
    // analysis of sampling profiler data and tab_search:top100:2020).
    RAW_PTR_EXCLUSION const void* const identifier;

    // Whether the task was at some point in a queue that was blocked or low
    // priority.
    const bool was_blocked_or_low_priority;

    // The time at which the task or event started running.
    const base::TimeTicks execution_start_time;

    // Whether the task or event has caused reentrancy.
    bool caused_reentrancy = false;
  };

  // This is called when |metric_source_| finishes destruction.
  void FinishDestroyMetricSource();

  // Common implementations for the thread-specific methods.
  void WillRunTask(const base::PendingTask* task,
                   bool was_blocked_or_low_priority,
                   std::vector<Metadata>* currently_running_metadata);

  // |callback| will either be synchronously invoked, or else never invoked.
  using TaskOrEventFinishedCallback = base::OnceCallback<
      void(base::TimeTicks, base::TimeTicks, base::TimeTicks)>;
  void DidRunTask(const base::PendingTask* task,
                  std::vector<Metadata>* currently_running_metadata,
                  int* mismatched_task_identifiers,
                  TaskOrEventFinishedCallback callback);

  // The source that emits responsiveness events.
  std::unique_ptr<MetricSource> metric_source_;

  // The following members are all affine to the UI thread.
  std::unique_ptr<Calculator> calculator_;

  // Metadata for currently running tasks and events on the UI thread.
  std::vector<Metadata> currently_running_metadata_ui_;

  // Task identifiers should only be mismatched once, since the Watcher may
  // register itself during a Task execution, and thus doesn't capture the
  // initial WillRunTask() callback.
  int mismatched_task_identifiers_ui_ = 0;

  // Event identifiers should be mismatched at most once, since the Watcher may
  // register itself during an event execution, and thus doesn't capture the
  // initial WillRunEventOnUIThread callback.
  int mismatched_event_identifiers_ui_ = 0;

  // The following members are all affine to the IO thread.
  std::vector<Metadata> currently_running_metadata_io_;
  int mismatched_task_identifiers_io_ = 0;

  // The implementation of this class guarantees that |calculator_io_| will be
  // non-nullptr and point to a valid object any time it is used on the IO
  // thread. To ensure this, the first task that this class posts onto the IO
  // thread sets |calculator_io_|. On destruction, this class first tears down
  // all consumers of |calculator_io_|, and then clears the member and destroys
  // Calculator.
  // `calculator_io_` is not a raw_ptr<...> because Calculator isn't supported
  // in raw_ptr for performance reasons. See crbug.com/1287151.
  RAW_PTR_EXCLUSION Calculator* calculator_io_ = nullptr;
};

}  // namespace responsiveness
}  // namespace content

#endif  // CONTENT_BROWSER_SCHEDULER_RESPONSIVENESS_WATCHER_H_

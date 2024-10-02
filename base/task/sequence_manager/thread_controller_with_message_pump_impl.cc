// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/thread_controller_with_message_pump_impl.h"

#include <algorithm>
#include <atomic>
#include <utility>

#include "base/auto_reset.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/message_loop/message_pump.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequence_manager/tasks.h"
#include "base/task/task_features.h"
#include "base/threading/hang_watcher.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_IOS)
#include "base/message_loop/message_pump_mac.h"
#elif BUILDFLAG(IS_ANDROID)
#include "base/message_loop/message_pump_android.h"
#endif

namespace base {
namespace sequence_manager {
namespace internal {
namespace {

// Returns |next_run_time| capped at 1 day from |lazy_now|. This is used to
// mitigate https://crbug.com/850450 where some platforms are unhappy with
// delays > 100,000,000 seconds. In practice, a diagnosis metric showed that no
// sleep > 1 hour ever completes (always interrupted by an earlier MessageLoop
// event) and 99% of completed sleeps are the ones scheduled for <= 1 second.
// Details @ https://crrev.com/c/1142589.
TimeTicks CapAtOneDay(TimeTicks next_run_time, LazyNow* lazy_now) {
  return std::min(next_run_time, lazy_now->Now() + Days(1));
}

// Feature to run tasks by batches before pumping out messages.
BASE_FEATURE(kRunTasksByBatches,
             "RunTasksByBatches",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// If enabled, deactivate the high resolution timer immediately in DoWork(),
// instead of waiting for next DoIdleWork.
BASE_FEATURE(kUseLessHighResTimers,
             "UseLessHighResTimers",
             base::FEATURE_DISABLED_BY_DEFAULT);
std::atomic_bool g_use_less_high_res_timers = false;

// If enabled, high resolution timer will be used all the time on Windows. This
// is for test only.
BASE_FEATURE(kAlwaysUseHighResTimers,
             "AlwaysUseHighResTimers",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

std::atomic_bool g_align_wake_ups = false;
std::atomic_bool g_run_tasks_by_batches = false;
#if BUILDFLAG(IS_WIN)
bool g_explicit_high_resolution_timer_win = false;
#endif  // BUILDFLAG(IS_WIN)

TimeTicks WakeUpRunTime(const WakeUp& wake_up) {
  // Windows relies on the low resolution timer rather than manual wake up
  // alignment.
#if BUILDFLAG(IS_WIN)
  if (g_explicit_high_resolution_timer_win)
    return wake_up.earliest_time();
#else  // BUILDFLAG(IS_WIN)
  if (g_align_wake_ups.load(std::memory_order_relaxed)) {
    TimeTicks aligned_run_time = wake_up.earliest_time().SnappedToNextTick(
        TimeTicks(), GetTaskLeewayForCurrentThread());
    return std::min(aligned_run_time, wake_up.latest_time());
  }
#endif
  return wake_up.time;
}

}  // namespace

// static
void ThreadControllerWithMessagePumpImpl::InitializeFeatures() {
  g_align_wake_ups = FeatureList::IsEnabled(kAlignWakeUps);
  g_run_tasks_by_batches.store(FeatureList::IsEnabled(kRunTasksByBatches),
                               std::memory_order_relaxed);
#if BUILDFLAG(IS_WIN)
  g_explicit_high_resolution_timer_win =
      FeatureList::IsEnabled(kExplicitHighResolutionTimerWin);
  g_use_less_high_res_timers.store(
      FeatureList::IsEnabled(kUseLessHighResTimers), std::memory_order_relaxed);
  if (FeatureList::IsEnabled(kAlwaysUseHighResTimers)) {
    Time::ActivateHighResolutionTimer(true);
  }
#endif
}

// static
void ThreadControllerWithMessagePumpImpl::ResetFeatures() {
  g_align_wake_ups.store(
      kAlignWakeUps.default_state == FEATURE_ENABLED_BY_DEFAULT,
      std::memory_order_relaxed);
  g_run_tasks_by_batches.store(
      kRunTasksByBatches.default_state == FEATURE_ENABLED_BY_DEFAULT,
      std::memory_order_relaxed);
}

ThreadControllerWithMessagePumpImpl::ThreadControllerWithMessagePumpImpl(
    const SequenceManager::Settings& settings)
    : ThreadController(settings.clock),
      work_deduplicator_(associated_thread_),
      can_run_tasks_by_batches_(settings.can_run_tasks_by_batches) {}

ThreadControllerWithMessagePumpImpl::ThreadControllerWithMessagePumpImpl(
    std::unique_ptr<MessagePump> message_pump,
    const SequenceManager::Settings& settings)
    : ThreadControllerWithMessagePumpImpl(settings) {
  BindToCurrentThread(std::move(message_pump));
}

ThreadControllerWithMessagePumpImpl::~ThreadControllerWithMessagePumpImpl() {
  // Destructors of MessagePump::Delegate and
  // SingleThreadTaskRunner::CurrentDefaultHandle will do all the clean-up.
  // ScopedSetSequenceLocalStorageMapForCurrentThread destructor will
  // de-register the current thread as a sequence.

#if BUILDFLAG(IS_WIN)
  if (main_thread_only().in_high_res_mode) {
    main_thread_only().in_high_res_mode = false;
    Time::ActivateHighResolutionTimer(false);
  }
#endif
}

// static
std::unique_ptr<ThreadControllerWithMessagePumpImpl>
ThreadControllerWithMessagePumpImpl::CreateUnbound(
    const SequenceManager::Settings& settings) {
  return base::WrapUnique(new ThreadControllerWithMessagePumpImpl(settings));
}

ThreadControllerWithMessagePumpImpl::MainThreadOnly::MainThreadOnly() = default;

ThreadControllerWithMessagePumpImpl::MainThreadOnly::~MainThreadOnly() =
    default;

void ThreadControllerWithMessagePumpImpl::SetSequencedTaskSource(
    SequencedTaskSource* task_source) {
  DCHECK(task_source);
  DCHECK(!main_thread_only().task_source);
  main_thread_only().task_source = task_source;
}

void ThreadControllerWithMessagePumpImpl::BindToCurrentThread(
    std::unique_ptr<MessagePump> message_pump) {
  associated_thread_->BindToCurrentThread();
  pump_ = std::move(message_pump);
  work_id_provider_ = WorkIdProvider::GetForCurrentThread();
  RunLoop::RegisterDelegateForCurrentThread(this);
  scoped_set_sequence_local_storage_map_for_current_thread_ = std::make_unique<
      base::internal::ScopedSetSequenceLocalStorageMapForCurrentThread>(
      &sequence_local_storage_map_);
  {
    base::internal::CheckedAutoLock task_runner_lock(task_runner_lock_);
    if (task_runner_)
      InitializeSingleThreadTaskRunnerCurrentDefaultHandle();
  }
  if (work_deduplicator_.BindToCurrentThread() ==
      ShouldScheduleWork::kScheduleImmediate) {
    pump_->ScheduleWork();
  }
}

void ThreadControllerWithMessagePumpImpl::SetWorkBatchSize(
    int work_batch_size) {
  DCHECK_GE(work_batch_size, 1);
  CHECK(main_thread_only().can_change_batch_size);
  main_thread_only().work_batch_size = work_batch_size;
}

void ThreadControllerWithMessagePumpImpl::SetTimerSlack(
    TimerSlack timer_slack) {
  DCHECK(RunsTasksInCurrentSequence());
  pump_->SetTimerSlack(timer_slack);
}

void ThreadControllerWithMessagePumpImpl::WillQueueTask(
    PendingTask* pending_task) {
  task_annotator_.WillQueueTask("SequenceManager PostTask", pending_task);
}

void ThreadControllerWithMessagePumpImpl::ScheduleWork() {
  base::internal::CheckedLock::AssertNoLockHeldOnCurrentThread();
  if (work_deduplicator_.OnWorkRequested() ==
      ShouldScheduleWork::kScheduleImmediate) {
    if (!associated_thread_->IsBoundToCurrentThread()) {
      run_level_tracker_.RecordScheduleWork();
    } else {
      TRACE_EVENT_INSTANT("wakeup.flow", "ScheduleWorkToSelf");
    }
    pump_->ScheduleWork();
  }
}

void ThreadControllerWithMessagePumpImpl::SetNextDelayedDoWork(
    LazyNow* lazy_now,
    absl::optional<WakeUp> wake_up) {
  DCHECK(!wake_up || !wake_up->is_immediate());
  TimeTicks run_time =
      wake_up.has_value() ? WakeUpRunTime(*wake_up) : TimeTicks::Max();
  DCHECK_LT(lazy_now->Now(), run_time);

  if (main_thread_only().next_delayed_do_work == run_time)
    return;
  main_thread_only().next_delayed_do_work = run_time;

  // It's very rare for PostDelayedTask to be called outside of a DoWork in
  // production, so most of the time this does nothing.
  if (work_deduplicator_.OnDelayedWorkRequested() ==
      ShouldScheduleWork::kScheduleImmediate) {
    // Cap at one day but remember the exact time for the above equality check
    // on the next round.
    if (!run_time.is_max())
      run_time = CapAtOneDay(run_time, lazy_now);
    // |pump_| can't be null as all postTasks are cross-thread before binding,
    // and delayed cross-thread postTasks do the thread hop through an immediate
    // task.
    pump_->ScheduleDelayedWork({run_time, lazy_now->Now()});
  }
}

bool ThreadControllerWithMessagePumpImpl::RunsTasksInCurrentSequence() {
  return associated_thread_->IsBoundToCurrentThread();
}

void ThreadControllerWithMessagePumpImpl::SetDefaultTaskRunner(
    scoped_refptr<SingleThreadTaskRunner> task_runner) {
  base::internal::CheckedAutoLock lock(task_runner_lock_);
  task_runner_ = task_runner;
  if (associated_thread_->IsBound()) {
    DCHECK(associated_thread_->IsBoundToCurrentThread());
    // Thread task runner handle will be created in BindToCurrentThread().
    InitializeSingleThreadTaskRunnerCurrentDefaultHandle();
  }
}

void ThreadControllerWithMessagePumpImpl::
    InitializeSingleThreadTaskRunnerCurrentDefaultHandle() {
  // Only one SingleThreadTaskRunner::CurrentDefaultHandle can exist at any
  // time, so reset the old one.
  main_thread_only().thread_task_runner_handle.reset();
  main_thread_only().thread_task_runner_handle =
      std::make_unique<SingleThreadTaskRunner::CurrentDefaultHandle>(
          task_runner_);
  // When the task runner is known, bind the power manager. Power notifications
  // are received through that sequence.
  power_monitor_.BindToCurrentThread();
}

scoped_refptr<SingleThreadTaskRunner>
ThreadControllerWithMessagePumpImpl::GetDefaultTaskRunner() {
  base::internal::CheckedAutoLock lock(task_runner_lock_);
  return task_runner_;
}

void ThreadControllerWithMessagePumpImpl::RestoreDefaultTaskRunner() {
  // There is no default task runner (as opposed to ThreadControllerImpl).
}

void ThreadControllerWithMessagePumpImpl::AddNestingObserver(
    RunLoop::NestingObserver* observer) {
  DCHECK(!main_thread_only().nesting_observer);
  DCHECK(observer);
  main_thread_only().nesting_observer = observer;
  RunLoop::AddNestingObserverOnCurrentThread(this);
}

void ThreadControllerWithMessagePumpImpl::RemoveNestingObserver(
    RunLoop::NestingObserver* observer) {
  DCHECK_EQ(main_thread_only().nesting_observer, observer);
  main_thread_only().nesting_observer = nullptr;
  RunLoop::RemoveNestingObserverOnCurrentThread(this);
}

void ThreadControllerWithMessagePumpImpl::OnBeginWorkItem() {
  LazyNow lazy_now(time_source_);
  OnBeginWorkItemImpl(lazy_now);
}

void ThreadControllerWithMessagePumpImpl::OnBeginWorkItemImpl(
    LazyNow& lazy_now) {
  hang_watch_scope_.emplace();
  work_id_provider_->IncrementWorkId();
  run_level_tracker_.OnWorkStarted(lazy_now);
}

void ThreadControllerWithMessagePumpImpl::OnEndWorkItem(int run_level_depth) {
  LazyNow lazy_now(time_source_);
  OnEndWorkItemImpl(lazy_now, run_level_depth);
}

void ThreadControllerWithMessagePumpImpl::OnEndWorkItemImpl(
    LazyNow& lazy_now,
    int run_level_depth) {
  // Work completed, begin a new hang watch until the next task (watching the
  // pump's overhead).
  hang_watch_scope_.emplace();
  work_id_provider_->IncrementWorkId();
  run_level_tracker_.OnWorkEnded(lazy_now, run_level_depth);
}

void ThreadControllerWithMessagePumpImpl::BeforeWait() {
  // In most cases, DoIdleWork() will already have cleared the
  // `hang_watch_scope_` but in some cases where the native side of the
  // MessagePump impl is instrumented, it's possible to get a BeforeWait()
  // outside of a DoWork cycle (e.g. message_pump_win.cc :
  // MessagePumpForUI::HandleWorkMessage).
  hang_watch_scope_.reset();

  work_id_provider_->IncrementWorkId();
  LazyNow lazy_now(time_source_);
  run_level_tracker_.OnIdle(lazy_now);
}

MessagePump::Delegate::NextWorkInfo
ThreadControllerWithMessagePumpImpl::DoWork() {
#if BUILDFLAG(IS_WIN)
  // We've been already in a wakeup here. Deactivate the high res timer of OS
  // immediately instead of waiting for next DoIdleWork().
  if (g_use_less_high_res_timers.load(std::memory_order_relaxed) &&
      main_thread_only().in_high_res_mode) {
    main_thread_only().in_high_res_mode = false;
    Time::ActivateHighResolutionTimer(false);
  }
#endif
  MessagePump::Delegate::NextWorkInfo next_work_info{};

  work_deduplicator_.OnWorkStarted();
  LazyNow continuation_lazy_now(time_source_);
  absl::optional<WakeUp> next_wake_up = DoWorkImpl(&continuation_lazy_now);

  // If we are yielding after DoWorkImpl (a work batch) set the flag boolean.
  // This will inform the MessagePump to schedule a new continuation based on
  // the information below, but even if its immediate let the native sequence
  // have a chance to run.
  // When we have |g_run_tasks_by_batches| active we want to always set the flag
  // to true to have a similar behavior on Android as on the desktop platforms
  // for this experiment.
  if (RunsTasksByBatches() ||
      (!main_thread_only().yield_to_native_after_batch.is_null() &&
       continuation_lazy_now.Now() <
           main_thread_only().yield_to_native_after_batch)) {
    next_work_info.yield_to_native = true;
  }
  // Schedule a continuation.
  WorkDeduplicator::NextTask next_task =
      (next_wake_up && next_wake_up->is_immediate())
          ? WorkDeduplicator::NextTask::kIsImmediate
          : WorkDeduplicator::NextTask::kIsDelayed;
  if (work_deduplicator_.DidCheckForMoreWork(next_task) ==
      ShouldScheduleWork::kScheduleImmediate) {
    // Need to run new work immediately, but due to the contract of DoWork
    // we only need to return a null TimeTicks to ensure that happens.
    return next_work_info;
  }

  // Special-casing here avoids unnecessarily sampling Now() when out of work.
  if (!next_wake_up) {
    main_thread_only().next_delayed_do_work = TimeTicks::Max();
    next_work_info.delayed_run_time = TimeTicks::Max();
    return next_work_info;
  }

  // The MessagePump will schedule the wake up on our behalf, so we need to
  // update |main_thread_only().next_delayed_do_work|.
  main_thread_only().next_delayed_do_work = WakeUpRunTime(*next_wake_up);

  // Don't request a run time past |main_thread_only().quit_runloop_after|.
  if (main_thread_only().next_delayed_do_work >
      main_thread_only().quit_runloop_after) {
    main_thread_only().next_delayed_do_work =
        main_thread_only().quit_runloop_after;
    // If we've passed |quit_runloop_after| there's no more work to do.
    if (continuation_lazy_now.Now() >= main_thread_only().quit_runloop_after) {
      next_work_info.delayed_run_time = TimeTicks::Max();
      return next_work_info;
    }
  }

  next_work_info.delayed_run_time = CapAtOneDay(
      main_thread_only().next_delayed_do_work, &continuation_lazy_now);
  next_work_info.recent_now = continuation_lazy_now.Now();
  return next_work_info;
}

absl::optional<WakeUp> ThreadControllerWithMessagePumpImpl::DoWorkImpl(
    LazyNow* continuation_lazy_now) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("sequence_manager"),
               "ThreadControllerImpl::DoWork");

  if (!main_thread_only().task_execution_allowed) {
    // Broadcast in a trace event that application tasks were disallowed. This
    // helps spot nested loops that intentionally starve application tasks.
    TRACE_EVENT0("base", "ThreadController: application tasks disallowed");
    if (main_thread_only().quit_runloop_after == TimeTicks::Max())
      return absl::nullopt;
    return WakeUp{main_thread_only().quit_runloop_after};
  }

  DCHECK(main_thread_only().task_source);

  // Keep running tasks for up to 8ms before yielding to the pump when tasks are
  // run by batches.
  const base::TimeDelta batch_duration =
      RunsTasksByBatches() ? base::Milliseconds(8) : base::Milliseconds(0);

  const absl::optional<base::TimeTicks> start_time =
      batch_duration.is_zero()
          ? absl::nullopt
          : absl::optional<base::TimeTicks>(time_source_->NowTicks());
  absl::optional<base::TimeTicks> recent_time = start_time;

  // Loops for |batch_duration|, or |work_batch_size| times if |batch_duration|
  // is zero.
  for (int num_tasks_executed = 0;
       (!batch_duration.is_zero() &&
        (recent_time.value() - start_time.value()) < batch_duration) ||
       (batch_duration.is_zero() &&
        num_tasks_executed < main_thread_only().work_batch_size);
       ++num_tasks_executed) {
    LazyNow lazy_now_select_task(recent_time, time_source_);
    // Include SelectNextTask() in the scope of the work item. This ensures
    // it's covered in tracing and hang reports. This is particularly
    // important when SelectNextTask() finds no work immediately after a
    // wakeup, otherwise the power-inefficient wakeup is invisible in
    // tracing. OnApplicationTaskSelected() assumes this ordering as well.
    OnBeginWorkItemImpl(lazy_now_select_task);
    int run_depth = static_cast<int>(run_level_tracker_.num_run_levels());

    const SequencedTaskSource::SelectTaskOption select_task_option =
        power_monitor_.IsProcessInPowerSuspendState()
            ? SequencedTaskSource::SelectTaskOption::kSkipDelayedTask
            : SequencedTaskSource::SelectTaskOption::kDefault;
    absl::optional<SequencedTaskSource::SelectedTask> selected_task =
        main_thread_only().task_source->SelectNextTask(lazy_now_select_task,
                                                       select_task_option);
    LazyNow lazy_now_task_selected(time_source_);
    run_level_tracker_.OnApplicationTaskSelected(
        (selected_task && selected_task->task.delayed_run_time.is_null())
            ? selected_task->task.queue_time
            : TimeTicks(),
        lazy_now_task_selected);
    if (!selected_task) {
      OnEndWorkItemImpl(lazy_now_task_selected, run_depth);
      break;
    }

    // Execute the task and assume the worst: it is probably not reentrant.
    AutoReset<bool> ban_nested_application_tasks(
        &main_thread_only().task_execution_allowed, false);

    // Trace-parsing tools (DevTools, Lighthouse, etc) consume this event to
    // determine long tasks.
    // See https://crbug.com/681863 and https://crbug.com/874982
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "RunTask");

    {
      // Always track the start of the task, as this is low-overhead.
      TaskAnnotator::LongTaskTracker long_task_tracker(
          time_source_, selected_task->task, &task_annotator_);

      // Note: all arguments after task are just passed to a TRACE_EVENT for
      // logging so lambda captures are safe as lambda is executed inline.
      SequencedTaskSource* source = main_thread_only().task_source;
      task_annotator_.RunTask(
          "ThreadControllerImpl::RunTask", selected_task->task,
          [&selected_task, &source](perfetto::EventContext& ctx) {
            if (selected_task->task_execution_trace_logger) {
              selected_task->task_execution_trace_logger.Run(
                  ctx, selected_task->task);
            }
            source->MaybeEmitTaskDetails(ctx, selected_task.value());
          });
    }

    // Reset `selected_task` before the call to `DidRunTask()` below makes its
    // `PendingTask` reference dangling.
    selected_task.reset();

    LazyNow lazy_now_after_run_task(time_source_);
    main_thread_only().task_source->DidRunTask(lazy_now_after_run_task);
    // End the work item scope after DidRunTask() as it can process microtasks
    // (which are extensions of the RunTask).
    OnEndWorkItemImpl(lazy_now_after_run_task, run_depth);

    // If DidRunTask() read the clock (lazy_now_after_run_task.has_value()) or
    // if |batch_duration| > 0, store the clock value in `recent_time` so it can
    // be reused by SelectNextTask() at the next loop iteration.
    if (lazy_now_after_run_task.has_value() || !batch_duration.is_zero()) {
      recent_time = lazy_now_after_run_task.Now();
    } else {
      recent_time.reset();
    }

    // When Quit() is called we must stop running the batch because the
    // caller expects per-task granularity.
    if (main_thread_only().quit_pending)
      break;
  }

  if (main_thread_only().quit_pending)
    return absl::nullopt;

  work_deduplicator_.WillCheckForMoreWork();

  // Re-check the state of the power after running tasks. An executed task may
  // have been a power change notification.
  const SequencedTaskSource::SelectTaskOption select_task_option =
      power_monitor_.IsProcessInPowerSuspendState()
          ? SequencedTaskSource::SelectTaskOption::kSkipDelayedTask
          : SequencedTaskSource::SelectTaskOption::kDefault;
  main_thread_only().task_source->RemoveAllCanceledDelayedTasksFromFront(
      continuation_lazy_now);
  return main_thread_only().task_source->GetPendingWakeUp(continuation_lazy_now,
                                                          select_task_option);
}

bool ThreadControllerWithMessagePumpImpl::RunsTasksByBatches() const {
  return can_run_tasks_by_batches_ &&
         g_run_tasks_by_batches.load(std::memory_order_relaxed);
}

bool ThreadControllerWithMessagePumpImpl::DoIdleWork() {
  struct OnIdle {
    OnIdle(const TickClock* time_source, RunLevelTracker& run_level_tracker_ref)
        : lazy_now(time_source), run_level_tracker(run_level_tracker_ref) {}

    // Very last step before going idle, must be fast as this is hidden from the
    // DoIdleWork trace event below.
    ~OnIdle() { run_level_tracker->OnIdle(lazy_now); }

    LazyNow lazy_now;

   private:
    const raw_ref<RunLevelTracker> run_level_tracker;
  };
  absl::optional<OnIdle> on_idle;

  // Must be after `on_idle` as this trace event's scope must end before the END
  // of the "ThreadController active" trace event emitted from
  // `run_level_tracker_.OnIdle()`.
  TRACE_EVENT0("sequence_manager", "SequenceManager::DoIdleWork");

#if BUILDFLAG(IS_WIN)
  if (!power_monitor_.IsProcessInPowerSuspendState()) {
    // Avoid calling Time::ActivateHighResolutionTimer() between
    // suspend/resume as the system hangs if we do (crbug.com/1074028).
    // OnResume() will generate a task on this thread per the
    // ThreadControllerPowerMonitor observer and DoIdleWork() will thus get
    // another chance to set the right high-resolution-timer-state before
    // going to sleep after resume.

    const bool need_high_res_mode =
        main_thread_only().task_source->HasPendingHighResolutionTasks();
    if (main_thread_only().in_high_res_mode != need_high_res_mode) {
      // On Windows we activate the high resolution timer so that the wait
      // _if_ triggered by the timer happens with good resolution. If we don't
      // do this the default resolution is 15ms which might not be acceptable
      // for some tasks.
      main_thread_only().in_high_res_mode = need_high_res_mode;
      Time::ActivateHighResolutionTimer(need_high_res_mode);
    }
  }
#endif  // BUILDFLAG(IS_WIN)

  if (main_thread_only().task_source->OnSystemIdle()) {
    // The OnSystemIdle() callback resulted in more immediate work, so schedule
    // a DoWork callback. For some message pumps returning true from here is
    // sufficient to do that but not on mac.
    pump_->ScheduleWork();
    return false;
  }

  // This is mostly redundant with the identical call in BeforeWait (upcoming)
  // but some uninstrumented MessagePump impls don't call BeforeWait so it must
  // also be done here.
  hang_watch_scope_.reset();

  // All return paths below are truly idle.
  on_idle.emplace(time_source_, run_level_tracker_);

  // Check if any runloop timeout has expired.
  if (main_thread_only().quit_runloop_after != TimeTicks::Max() &&
      main_thread_only().quit_runloop_after <= on_idle->lazy_now.Now()) {
    Quit();
    return false;
  }

  // RunLoop::Delegate knows whether we called Run() or RunUntilIdle().
  if (ShouldQuitWhenIdle())
    Quit();

  return false;
}

int ThreadControllerWithMessagePumpImpl::RunDepth() {
  return static_cast<int>(run_level_tracker_.num_run_levels());
}

void ThreadControllerWithMessagePumpImpl::Run(bool application_tasks_allowed,
                                              TimeDelta timeout) {
  DCHECK(RunsTasksInCurrentSequence());

  LazyNow lazy_now_run_loop_start(time_source_);

  // RunLoops can be nested so we need to restore the previous value of
  // |quit_runloop_after| upon exit. NB we could use saturated arithmetic here
  // but don't because we have some tests which assert the number of calls to
  // Now.
  AutoReset<TimeTicks> quit_runloop_after(
      &main_thread_only().quit_runloop_after,
      (timeout == TimeDelta::Max()) ? TimeTicks::Max()
                                    : lazy_now_run_loop_start.Now() + timeout);

  run_level_tracker_.OnRunLoopStarted(RunLevelTracker::kInBetweenWorkItems,
                                      lazy_now_run_loop_start);

  // Quit may have been called outside of a Run(), so |quit_pending| might be
  // true here. We can't use InTopLevelDoWork() in Quit() as this call may be
  // outside top-level DoWork but still in Run().
  main_thread_only().quit_pending = false;
  hang_watch_scope_.emplace();
  if (application_tasks_allowed && !main_thread_only().task_execution_allowed) {
    // Allow nested task execution as explicitly requested.
    DCHECK(RunLoop::IsNestedOnCurrentThread());
    main_thread_only().task_execution_allowed = true;
    pump_->Run(this);
    main_thread_only().task_execution_allowed = false;
  } else {
    pump_->Run(this);
  }

  run_level_tracker_.OnRunLoopEnded();
  main_thread_only().quit_pending = false;

  // If this was a nested loop, hang watch the remainder of the task which
  // caused it. Otherwise, stop watching as we're no longer running.
  if (RunLoop::IsNestedOnCurrentThread()) {
    hang_watch_scope_.emplace();
  } else {
    hang_watch_scope_.reset();
  }
  work_id_provider_->IncrementWorkId();
}

void ThreadControllerWithMessagePumpImpl::OnBeginNestedRunLoop() {
  // We don't need to ScheduleWork here! That's because the call to pump_->Run()
  // above, which is always called for RunLoop().Run(), guarantees a call to
  // DoWork on all platforms.
  if (main_thread_only().nesting_observer)
    main_thread_only().nesting_observer->OnBeginNestedRunLoop();
}

void ThreadControllerWithMessagePumpImpl::OnExitNestedRunLoop() {
  if (main_thread_only().nesting_observer)
    main_thread_only().nesting_observer->OnExitNestedRunLoop();
}

void ThreadControllerWithMessagePumpImpl::Quit() {
  DCHECK(RunsTasksInCurrentSequence());
  // Interrupt a batch of work.
  main_thread_only().quit_pending = true;

  // If we're in a nested RunLoop, continuation will be posted if necessary.
  pump_->Quit();
}

void ThreadControllerWithMessagePumpImpl::EnsureWorkScheduled() {
  if (work_deduplicator_.OnWorkRequested() ==
      ShouldScheduleWork::kScheduleImmediate) {
    pump_->ScheduleWork();
  }
}

void ThreadControllerWithMessagePumpImpl::SetTaskExecutionAllowed(
    bool allowed) {
  if (allowed) {
    // We need to schedule work unconditionally because we might be about to
    // enter an OS level nested message loop. Unlike a RunLoop().Run() we don't
    // get a call to DoWork on entering for free.
    work_deduplicator_.OnWorkRequested();  // Set the pending DoWork flag.
    pump_->ScheduleWork();
  } else {
    // We've (probably) just left an OS level nested message loop. Make sure a
    // subsequent PostTask within the same Task doesn't ScheduleWork with the
    // pump (this will be done anyway when the task exits).
    work_deduplicator_.OnWorkStarted();
  }
  main_thread_only().task_execution_allowed = allowed;
}

bool ThreadControllerWithMessagePumpImpl::IsTaskExecutionAllowed() const {
  return main_thread_only().task_execution_allowed;
}

MessagePump* ThreadControllerWithMessagePumpImpl::GetBoundMessagePump() const {
  return pump_.get();
}

void ThreadControllerWithMessagePumpImpl::PrioritizeYieldingToNative(
    base::TimeTicks prioritize_until) {
  main_thread_only().yield_to_native_after_batch = prioritize_until;
}

#if BUILDFLAG(IS_IOS)
void ThreadControllerWithMessagePumpImpl::AttachToMessagePump() {
  static_cast<MessagePumpCFRunLoopBase*>(pump_.get())->Attach(this);
}

void ThreadControllerWithMessagePumpImpl::DetachFromMessagePump() {
  static_cast<MessagePumpCFRunLoopBase*>(pump_.get())->Detach();
}
#elif BUILDFLAG(IS_ANDROID)
void ThreadControllerWithMessagePumpImpl::AttachToMessagePump() {
  CHECK(main_thread_only().work_batch_size == 1);
  // Aborting the message pump currently relies on the batch size being 1.
  main_thread_only().can_change_batch_size = false;
  static_cast<MessagePumpForUI*>(pump_.get())->Attach(this);
}
#endif

bool ThreadControllerWithMessagePumpImpl::ShouldQuitRunLoopWhenIdle() {
  if (run_level_tracker_.num_run_levels() == 0)
    return false;
  // It's only safe to call ShouldQuitWhenIdle() when in a RunLoop.
  return ShouldQuitWhenIdle();
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

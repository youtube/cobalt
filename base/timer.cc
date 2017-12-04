// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/timer.h"

#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/platform_thread.h"
#include "nb/memory_scope.h"

namespace base {

// BaseTimerTaskInternal is a simple delegate for scheduling a callback to
// Timer in the thread's default task runner. It also handles the following
// edge cases:
// - deleted by the task runner.
// - abandoned (orphaned) by Timer.
class BaseTimerTaskInternal {
 public:
  BaseTimerTaskInternal(Timer* timer)
      : timer_(timer) {
  }

  ~BaseTimerTaskInternal() {
    // This task may be getting cleared because the task runner has been
    // destructed.  If so, don't leave Timer with a dangling pointer
    // to this.
    if (timer_)
      timer_->StopAndAbandon();
  }

  void Run() {
    TRACK_MEMORY_SCOPE("MessageLoop");
    // timer_ is NULL if we were abandoned.
    if (!timer_)
      return;

    // *this will be deleted by the task runner, so Timer needs to
    // forget us:
    timer_->scheduled_task_ = NULL;

    // Although Timer should not call back into *this, let's clear
    // the timer_ member first to be pedantic.
    Timer* timer = timer_;
    timer_ = NULL;
    timer->RunScheduledTask();
  }

  // The task remains in the MessageLoop queue, but nothing will happen when it
  // runs.
  void Abandon() {
    timer_ = NULL;
  }

 private:
  Timer* timer_;
};

Timer::Timer(bool retain_user_task,
             bool is_repeating,
             bool is_task_run_before_scheduling_next)
    : scheduled_task_(NULL),
      thread_id_(0),
      is_repeating_(is_repeating),
      is_task_run_before_scheduling_next_(is_task_run_before_scheduling_next),
      retain_user_task_(retain_user_task),
      is_running_(false) {
  if (is_task_run_before_scheduling_next_) {
    DCHECK(is_repeating_);
  }
}

Timer::Timer(const tracked_objects::Location& posted_from,
             TimeDelta delay,
             const base::Closure& user_task,
             bool is_repeating,
             bool is_task_run_before_scheduling_next)
    : scheduled_task_(NULL),
      posted_from_(posted_from),
      delay_(delay),
      user_task_(user_task),
      thread_id_(0),
      is_repeating_(is_repeating),
      is_task_run_before_scheduling_next_(is_task_run_before_scheduling_next),
      retain_user_task_(true),
      is_running_(false) {
  if (is_task_run_before_scheduling_next_) {
    DCHECK(is_repeating_);
  }
}

Timer::~Timer() {
  StopAndAbandon();
}

void Timer::Start(const tracked_objects::Location& posted_from,
                  TimeDelta delay,
                  const base::Closure& user_task) {
  SetTaskInfo(posted_from, delay, user_task);
  Reset();
}

void Timer::Stop() {
  is_running_ = false;
  if (!retain_user_task_)
    user_task_.Reset();
}

void Timer::Reset() {
  DCHECK(!user_task_.is_null());

  // If there's no pending task, start one up and return.
  if (!scheduled_task_) {
    PostNewScheduledTask(delay_);
    return;
  }

  // Set the new desired_run_time_.
  desired_run_time_ = TimeTicks::Now() + delay_;

  // We can use the existing scheduled task if it arrives before the new
  // desired_run_time_.
  if (desired_run_time_ > scheduled_run_time_) {
    is_running_ = true;
    return;
  }

  // We can't reuse the scheduled_task_, so abandon it and post a new one.
  AbandonScheduledTask();
  PostNewScheduledTask(delay_);
}

void Timer::SetTaskInfo(const tracked_objects::Location& posted_from,
                        TimeDelta delay,
                        const base::Closure& user_task) {
  posted_from_ = posted_from;
  delay_ = delay;
  user_task_ = user_task;
}

// This function is not re-implemented using SetupNewScheduledTask() and
// PostNewScheduledTask() to ensure that the default behavior of Timer is
// exactly the same as before.
void Timer::PostNewScheduledTask(TimeDelta delay) {
  TRACK_MEMORY_SCOPE("MessageLoop");
  DCHECK(scheduled_task_ == NULL);
  is_running_ = true;
  scheduled_task_ = new BaseTimerTaskInternal(this);
  ThreadTaskRunnerHandle::Get()->PostDelayedTask(posted_from_,
      base::Bind(&BaseTimerTaskInternal::Run, base::Owned(scheduled_task_)),
      delay);
  scheduled_run_time_ = desired_run_time_ = TimeTicks::Now() + delay;
  // Remember the thread ID that posts the first task -- this will be verified
  // later when the task is abandoned to detect misuse from multiple threads.
  if (!thread_id_)
    thread_id_ = static_cast<int>(PlatformThread::CurrentId());
}

Timer::NewScheduledTaskInfo Timer::SetupNewScheduledTask(
    TimeDelta expected_delay) {
  TRACK_MEMORY_SCOPE("MessageLoop");
  DCHECK(scheduled_task_ == NULL);
  DCHECK(is_task_run_before_scheduling_next_);
  DCHECK(thread_id_);

  is_running_ = true;
  scheduled_task_ = new BaseTimerTaskInternal(this);

  NewScheduledTaskInfo task_info;
  task_info.posted_from = posted_from_;
  task_info.task =
      base::Bind(&BaseTimerTaskInternal::Run, base::Owned(scheduled_task_));

  scheduled_run_time_ = desired_run_time_ = TimeTicks::Now() + expected_delay;

  return task_info;
}

void Timer::PostNewScheduledTask(NewScheduledTaskInfo task_info,
                                 TimeDelta delay) {
  TRACK_MEMORY_SCOPE("MessageLoop");
  // Some task runners expect a non-zero delay for PostDelayedTask.
  if (delay.ToInternalValue() == 0) {
    ThreadTaskRunnerHandle::Get()->PostTask(task_info.posted_from,
                                            task_info.task);
  } else {
    ThreadTaskRunnerHandle::Get()->PostDelayedTask(task_info.posted_from,
                                                   task_info.task, delay);
  }
}

void Timer::AbandonScheduledTask() {
  DCHECK(thread_id_ == 0 ||
         thread_id_ == static_cast<int>(PlatformThread::CurrentId()));
  if (scheduled_task_) {
    scheduled_task_->Abandon();
    scheduled_task_ = NULL;
  }
}

void Timer::RunScheduledTask() {
  TRACK_MEMORY_SCOPE("MessageLoop");
  // Task may have been disabled.
  if (!is_running_)
    return;

  // First check if we need to delay the task because of a new target time.
  if (desired_run_time_ > scheduled_run_time_) {
    // TimeTicks::Now() can be expensive, so only call it if we know the user
    // has changed the desired_run_time_.
    TimeTicks now = TimeTicks::Now();
    // Task runner may have called us late anyway, so only post a continuation
    // task if the desired_run_time_ is in the future.
    if (desired_run_time_ > now) {
      // Post a new task to span the remaining time.
      PostNewScheduledTask(desired_run_time_ - now);
      return;
    }
  }

  // Make a local copy of the task to run. The Stop method will reset the
  // user_task_ member if retain_user_task_ is false.
  base::Closure task = user_task_;

  if (!is_repeating_) {
    TRACK_MEMORY_SCOPE("MessageLoop");
    Stop();
    task.Run();
    return;
  }

  if (is_task_run_before_scheduling_next_) {
    TRACK_MEMORY_SCOPE("MessageLoop");
    // Setup member variables and the next tasks before the current one runs as
    // we cannot access any member variables after calling task.Run().
    NewScheduledTaskInfo task_info = SetupNewScheduledTask(delay_);
    base::TimeTicks task_start_time = base::TimeTicks::Now();
    task.Run();
    base::TimeDelta task_duration = base::TimeTicks::Now() - task_start_time;
    if (task_duration >= delay_) {
      PostNewScheduledTask(task_info, base::TimeDelta::FromInternalValue(0));
    } else {
      PostNewScheduledTask(task_info, delay_ - task_duration);
    }
  } else {
    TRACK_MEMORY_SCOPE("MessageLoop");
    PostNewScheduledTask(delay_);
    task.Run();
  }

  // No more member accesses here: *this could be deleted at this point.
}

}  // namespace base

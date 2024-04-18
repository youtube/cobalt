// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/web/window_timers.h"

#include <algorithm>
#include <limits>
#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/application_state.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/web/global_stats.h"

namespace cobalt {
namespace web {

int WindowTimers::TryAddNewTimer(Timer::TimerType type,
                                 const TimerCallbackArg& handler, int timeout) {
  int handle = GetFreeTimerHandle();
  DCHECK(handle);

  if (handle == 0) {  // unable to get a free timer handle
    // avoid accidentally overwriting existing timers
    return 0;
  }

  if (callbacks_active_) {
    scoped_refptr<Timer> timer =
        new Timer(type, owner_, stat_tracker_, debugger_hooks_, handler,
                  timeout, handle, this);
    if (application_state_ != base::kApplicationStateFrozen) {
      timer->StartOrResume();
    }
    timers_[handle] = timer;
  } else {
    timers_[handle] = nullptr;
  }

  return handle;
}

int WindowTimers::SetTimeout(const TimerCallbackArg& handler, int timeout) {
  LOG_IF(WARNING, timeout < 0)
      << "WindowTimers::SetTimeout received negative timeout: " << timeout;
  timeout = std::max(timeout, 0);

  return TryAddNewTimer(Timer::kOneShot, handler, timeout);
}

void WindowTimers::ClearTimeout(int handle) {
  Timers::iterator timer = timers_.find(handle);
  if (timer == timers_.end()) {
    return;
  }
  if (timer->second) {
    timer->second->Disable();
  }
  timers_.erase(timer);
}

int WindowTimers::SetInterval(const TimerCallbackArg& handler, int timeout) {
  LOG_IF(WARNING, timeout < 0)
      << "WindowTimers::SetInterval received negative interval: " << timeout;
  timeout = std::max(timeout, 0);

  return TryAddNewTimer(Timer::kRepeating, handler, timeout);
}

void WindowTimers::ClearInterval(int handle) { ClearTimeout(handle); }

void WindowTimers::DisableCallbacks() {
  callbacks_active_ = false;
  // Immediately cancel any pending timers.
  for (auto& timer_entry : timers_) {
    if (timer_entry.second) {
      timer_entry.second->Disable();
    }
    timer_entry.second = nullptr;
  }
}

int WindowTimers::GetFreeTimerHandle() {
  int next_timer_index = current_timer_index_;
  while (true) {
    if (next_timer_index == std::numeric_limits<int>::max()) {
      next_timer_index = 1;
    } else {
      ++next_timer_index;
    }
    if (timers_.find(next_timer_index) == timers_.end()) {
      current_timer_index_ = next_timer_index;
      return current_timer_index_;
    }
    if (next_timer_index == current_timer_index_) {
      break;
    }
  }
  DLOG(INFO) << "No available timer handle.";
  return 0;
}

void WindowTimers::SetApplicationState(base::ApplicationState state) {
  switch (state) {
    case base::kApplicationStateFrozen:
      DCHECK_EQ(application_state_, base::kApplicationStateConcealed);
      for (auto timer : timers_) {
        timer.second->Pause();
      }
      break;
    case base::kApplicationStateConcealed:
      if (application_state_ == base::kApplicationStateFrozen) {
        for (auto timer : timers_) {
          timer.second->StartOrResume();
        }
      }
      break;
    case base::kApplicationStateStopped:
    case base::kApplicationStateBlurred:
    case base::kApplicationStateStarted:
      break;
  }
  application_state_ = state;
}

WindowTimers::Timer::Timer(TimerType type, script::Wrappable* const owner,
                           web::StatTracker* stat_tracker,
                           const base::DebuggerHooks& debugger_hooks,
                           const TimerCallbackArg& callback, int timeout,
                           int handle, WindowTimers* window_timers)
    : type_(type),
      callback_(owner, callback),
      stat_tracker_(stat_tracker),
      debugger_hooks_(debugger_hooks),
      timeout_(timeout),
      handle_(handle),
      active_(false),
      window_timers_(window_timers) {
  debugger_hooks_.AsyncTaskScheduled(
      this, type == Timer::kOneShot ? "SetTimeout" : "SetInterval",
      type == Timer::kOneShot
          ? base::DebuggerHooks::AsyncTaskFrequency::kOneshot
          : base::DebuggerHooks::AsyncTaskFrequency::kRecurring);
  if (stat_tracker_) {
    switch (type) {
      case Timer::kOneShot:
        stat_tracker_->OnWindowTimersTimeoutCreated();
        break;
      case Timer::kRepeating:
        stat_tracker_->OnWindowTimersIntervalCreated();
        break;
    }
  }
}

WindowTimers::Timer::~Timer() {
  if (stat_tracker_) {
    switch (type_) {
      case Timer::kOneShot:
        stat_tracker_->OnWindowTimersTimeoutDestroyed();
        break;
      case Timer::kRepeating:
        stat_tracker_->OnWindowTimersIntervalDestroyed();
        break;
    }
  }
  debugger_hooks_.AsyncTaskCanceled(this);
}

void WindowTimers::Timer::Run() {
  if (!active_) {
    return;
  }

  // The callback is now being run. Track it in the global stats.
  web::GlobalStats::GetInstance()->StartJavaScriptEvent();

  {
    base::ScopedAsyncTask async_task(debugger_hooks_, this);
    callback_.value().Run();
  }

  // Remove one-shot timers from the timers list.
  if (active_ && !timer_->IsRunning()) {
    window_timers_->ClearTimeout(handle_);
  }

  // The callback has finished running. Stop tracking it in the global stats.
  web::GlobalStats::GetInstance()->StopJavaScriptEvent();
}

void WindowTimers::Timer::Pause() {
  if (timer_) {
    // The desired runtime is preserved here to determine whether the timer
    // should fire immediately when resuming.
    desired_run_time_ = timer_->desired_run_time();
    timer_.reset();
  }
}

void WindowTimers::Timer::StartOrResume() {
  if (timer_ != nullptr) return;
  active_ = true;
  switch (type_) {
    case kOneShot:
      if (desired_run_time_) {
        // Adjust the new timeout for the time spent while paused.
        auto now = base::TimeTicks::Now();
        if (desired_run_time_ <= now) {
          // The previous desired run time was in the past or is right now, so
          // it should fire immediately.
          timeout_ = 0;
        } else {
          // Set the timeout to keep the same desired run time. Note that since
          // the timer uses the timeout that we request to set a new desired
          // run time from the clock, the resumed timer will likely fire
          // slightly later.
          base::TimeDelta time_delta(desired_run_time_.value() - now);
          timeout_ = static_cast<int>(time_delta.InMilliseconds());
        }
      }
      timer_ = CreateAndStart<base::OneShotTimer>();
      break;
    case kRepeating:
      timer_ = CreateAndStart<base::RepeatingTimer>();
      if (timeout_ && desired_run_time_ &&
          desired_run_time_ < base::TimeTicks::Now()) {
        // The timer was paused and the desired run time is in the past.
        // Call the callback once before continuing the repeating timer.
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::Bind(&WindowTimers::Timer::Run, this));
      }
      break;
  }
}

void WindowTimers::Timer::Disable() {
  // Prevent the timer callback from doing anything.
  active_ = false;

  // Clear the DelayTimerBase object to release the reference to |this| in the
  // user callback.
  timer_.reset();
}

template <class TimerClass>
std::unique_ptr<base::internal::DelayTimerBase>
WindowTimers::Timer::CreateAndStart() {
  auto* timer = new TimerClass();
  timer->Start(FROM_HERE, base::TimeDelta::FromMilliseconds(timeout_),
               base::Bind(&WindowTimers::Timer::Run, this));
  return std::unique_ptr<base::internal::DelayTimerBase>(timer);
}

}  // namespace web
}  // namespace cobalt

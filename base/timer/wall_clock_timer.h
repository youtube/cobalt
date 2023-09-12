// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TIMER_WALL_CLOCK_TIMER_H_
#define BASE_TIMER_WALL_CLOCK_TIMER_H_

#include "base/base_export.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace base {
class Clock;
class TickClock;

// WallClockTimer is based on OneShotTimer and provides a simple timer API
// which is mostly similar to OneShotTimer's API. The main difference is that
// WallClockTimer is using Time (which is system-dependent) to schedule task.
// WallClockTimer calls you back once scheduled time has come.
//
// Comparison with OneShotTimer: WallClockTimer runs |user_task_| after |delay_|
// expires according to usual time, while OneShotTimer runs |user_task_| after
// |delay_| expires according to TimeTicks which may freeze on some platforms
// when power suspends (desktop falls asleep). On platforms where TimeTicks
// don't freeze, the WallClockTimer has the same behavior as OneShotTimer.
//
// The API is not thread safe. All methods must be called from the same
// sequence (not necessarily the construction sequence), except for the
// destructor.
// - The destructor may be called from any sequence when the timer is not
// running and there is no scheduled task active.
class BASE_EXPORT WallClockTimer : public PowerSuspendObserver {
 public:
  // Constructs a timer. Start() must be called later to start the timer.
  // If |clock| is provided, it's used instead of
  // DefaultClock::GetInstance() to calulate timer's delay. If |tick_clock|
  // is provided, it's used instead of TimeTicks::Now() to get TimeTicks when
  // scheduling tasks.
  WallClockTimer();
  WallClockTimer(const Clock* clock, const TickClock* tick_clock);
  WallClockTimer(const WallClockTimer&) = delete;
  WallClockTimer& operator=(const WallClockTimer&) = delete;

  ~WallClockTimer() override;

  // Starts the timer to run at the given |desired_run_time|. If the timer is
  // already running, it will be replaced to call the given |user_task|.
  virtual void Start(const Location& posted_from,
                     Time desired_run_time,
                     OnceClosure user_task);

  // Starts the timer to run at the given |desired_run_time|. If the timer is
  // already running, it will be replaced to call a task formed from
  // |receiver|->*|method|.
  template <class Receiver>
  void Start(const Location& posted_from,
             Time desired_run_time,
             Receiver* receiver,
             void (Receiver::*method)()) {
    Start(posted_from, desired_run_time,
          BindOnce(method, Unretained(receiver)));
  }

  // Stops the timer. It is a no-op if the timer is not running.
  void Stop();

  // Returns true if the timer is running.
  bool IsRunning() const;

  // PowerSuspendObserver:
  void OnResume() override;

  Time desired_run_time() const { return desired_run_time_; }

 private:
  void AddObserver();

  void RemoveObserver();

  // Actually run scheduled task
  void RunUserTask();

  // Returns the current time count.
  Time Now() const;

  bool observer_added_ = false;

  // Location in user code.
  Location posted_from_;

  // The desired run time of |user_task_|.
  Time desired_run_time_;

  OnceClosure user_task_;

  // Timer which should notify to run task in the period while system awake
  OneShotTimer timer_;

  // The clock used to calculate the run time for scheduled tasks.
  const raw_ptr<const Clock> clock_ = DefaultClock::GetInstance();
};

}  // namespace base

#endif  // BASE_TIMER_WALL_CLOCK_TIMER_H_

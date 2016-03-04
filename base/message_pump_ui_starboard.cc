// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "base/message_pump_ui_starboard.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/time.h"

#include "starboard/event.h"
#include "starboard/system.h"

namespace base {

namespace {

void CallMessagePump(void* context) {
  DCHECK(context);
  MessagePumpUIStarboard* pump =
      reinterpret_cast<MessagePumpUIStarboard*>(context);
  pump->RunOneAndReschedule();
}

}  // namespace

MessagePumpUIStarboard::MessagePumpUIStarboard() : delegate_(NULL) {}

void MessagePumpUIStarboard::RunOneAndReschedule() {
  DCHECK(delegate_);
  base::TimeTicks delayed_work_time;
  if (!RunOne(&delayed_work_time)) {
    return;
  }

  ScheduleDelayedWork(delayed_work_time);
}

void MessagePumpUIStarboard::Run(Delegate* delegate) {
  // This should never be called because we are not like a normal message pump
  // where we loop until told to quit. We are providing a MessagePump interface
  // on top of an externally-owned message pump. We want to exist and be able to
  // schedule work, but the actual for(;;) loop is owned by Starboard.
  NOTREACHED();
}

void MessagePumpUIStarboard::Start(Delegate* delegate) {
  run_loop_.reset(new base::RunLoop());
  delegate_ = delegate;

  // Since the RunLoop was just created above, BeforeRun should be guaranteed to
  // return true (it only returns false if the RunLoop has been Quit already).
  if (!run_loop_->BeforeRun())
    NOTREACHED();
}

void MessagePumpUIStarboard::Quit() {
  delegate_ = NULL;
  Cancel();
  if (run_loop_) {
    run_loop_->AfterRun();
    run_loop_.reset();
  }
}

void MessagePumpUIStarboard::ScheduleWork() {
  ScheduleIn(base::TimeDelta());
}

void MessagePumpUIStarboard::ScheduleDelayedWork(
    const base::TimeTicks& delayed_work_time) {
  base::TimeDelta delay;
  if (!delayed_work_time.is_null()) {
    delay = delayed_work_time - base::TimeTicks::Now();
  }

  ScheduleIn(delay);
}

void MessagePumpUIStarboard::ScheduleIn(const base::TimeDelta& delay) {
  base::AutoLock auto_lock(outstanding_events_lock_);
  // Make sure any outstanding scheduled event is canceled.
  CancelLocked();

  // See if this is a degenerate immediate case.
  if (delay <= base::TimeDelta()) {
    outstanding_events_.insert(SbEventSchedule(&CallMessagePump, this, 0));
    return;
  }

  outstanding_events_.insert(
      SbEventSchedule(&CallMessagePump, this, delay.ToSbTime()));
}

void MessagePumpUIStarboard::Cancel() {
  base::AutoLock auto_lock(outstanding_events_lock_);
  CancelLocked();
}

void MessagePumpUIStarboard::CancelLocked() {
  outstanding_events_lock_.AssertAcquired();
  for (SbEventIdSet::iterator it = outstanding_events_.begin();
       it != outstanding_events_.end(); ++it) {
    SbEventCancel(*it);
  }
  outstanding_events_.erase(outstanding_events_.begin(),
                            outstanding_events_.end());
}

bool MessagePumpUIStarboard::RunOne(TimeTicks* out_delayed_work_time) {
  DCHECK(out_delayed_work_time);
  DCHECK(delegate_);

  // Do any immediate work.
  bool did_immediate_work = delegate_->DoWork();

  // Do delayed work.
  base::TimeTicks delayed_work_time;
  bool did_delayed_work = false;
  // Unlike Chromium, we drain all due delayed work before going back to the
  // loop. See message_pump_io_starboard.cc for more information.
  while (delegate_->DoDelayedWork(&delayed_work_time)) {
    did_delayed_work = true;
  }

  // If we did immediate work, reschedule an immediate callback event.
  if (did_immediate_work) {
    // We don't set out_delayed_work_time here, because we want to be called
    // back immediately.
    return true;
  }

  // If we did any delayed work, schedule a callback event for when the next
  // delayed work item becomes due.
  if (did_delayed_work) {
    *out_delayed_work_time = delayed_work_time;
    return true;
  }

  // No work was done, so only call back if there was idle work done, otherwise
  // go to sleep. ScheduleWork or ScheduleDelayedWork will be called if new work
  // is scheduled.
  return delegate_->DoIdleWork();
}

}  // namespace base

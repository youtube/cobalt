// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "base/message_loop/message_pump_ui_starboard.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "starboard/event.h"
#include "starboard/system.h"

namespace base {

namespace {

void CallMessagePumpImmediate(void* context) {
  DCHECK(context);
  MessagePumpUIStarboard* pump =
      reinterpret_cast<MessagePumpUIStarboard*>(context);
  pump->RunOneAndReschedule(false /*delayed*/);
}

void CallMessagePumpDelayed(void* context) {
  DCHECK(context);
  MessagePumpUIStarboard* pump =
      reinterpret_cast<MessagePumpUIStarboard*>(context);
  pump->RunOneAndReschedule(true /*delayed*/);
}

}  // namespace

MessagePumpUIStarboard::MessagePumpUIStarboard() : delegate_(NULL) {}

void MessagePumpUIStarboard::RunOneAndReschedule(bool delayed) {
  DCHECK(delegate_);
  if (delayed) {
    CancelDelayed();
  } else {
    CancelImmediate();
  }

  TimeTicks delayed_work_time;
  for (;;) {
    TimeTicks next_time;
    if (!RunOne(&next_time)) {
      delayed_work_time = next_time;
      break;
    }
  }

  if (!delayed_work_time.is_null()) {
    Delegate::NextWorkInfo next_work_info;
    next_work_info.delayed_run_time = delayed_work_time;
    ScheduleDelayedWork(next_work_info);
  }
}

void MessagePumpUIStarboard::Run(Delegate* delegate) {
  // This should never be called because we are not like a normal message pump
  // where we loop until told to quit. We are providing a MessagePump interface
  // on top of an externally-owned message pump. We want to exist and be able to
  // schedule work, but the actual for(;;) loop is owned by Starboard.
  NOTREACHED();
}

void MessagePumpUIStarboard::Attach(Delegate* delegate) {
  // Since the Looper is controlled by the UI thread or JavaHandlerThread, we
  // can't use Run() like we do on other platforms or we would prevent Java
  // tasks from running. Instead we create and initialize a run loop here, then
  // return control back to the Looper.

  SetDelegate(delegate);
}

void MessagePumpUIStarboard::Quit() {
  delegate_ = NULL;
  CancelAll();
}

void MessagePumpUIStarboard::ScheduleWork() {
  base::AutoLock auto_lock(outstanding_events_lock_);
  if (!outstanding_events_.empty()) {
    // No need, already an outstanding event.
    return;
  }

  outstanding_events_.insert(
      SbEventSchedule(&CallMessagePumpImmediate, this, 0));
}

void MessagePumpUIStarboard::ScheduleDelayedWork(
    const Delegate::NextWorkInfo& next_work_info) {
  base::TimeDelta delay;
  if (!next_work_info.delayed_run_time.is_null()) {
    delay = next_work_info.delayed_run_time - base::TimeTicks::Now();

    if (delay <= base::TimeDelta()) {
      delay = base::TimeDelta();
    }
  }

  {
    base::AutoLock auto_lock(outstanding_events_lock_);
    // Make sure any outstanding delayed event is canceled.
    CancelDelayedLocked();

    outstanding_delayed_events_.insert(
        SbEventSchedule(&CallMessagePumpDelayed, this, delay.InMicroseconds()));
  }
}

void MessagePumpUIStarboard::CancelAll() {
  base::AutoLock auto_lock(outstanding_events_lock_);
  CancelImmediateLocked();
  CancelDelayedLocked();
}

void MessagePumpUIStarboard::CancelImmediate() {
  base::AutoLock auto_lock(outstanding_events_lock_);
  CancelImmediateLocked();
}

void MessagePumpUIStarboard::CancelDelayed() {
  base::AutoLock auto_lock(outstanding_events_lock_);
  CancelDelayedLocked();
}

void MessagePumpUIStarboard::CancelImmediateLocked() {
  outstanding_events_lock_.AssertAcquired();
  for (SbEventIdSet::iterator it = outstanding_events_.begin();
       it != outstanding_events_.end(); ++it) {
    SbEventCancel(*it);
  }
  outstanding_events_.erase(outstanding_events_.begin(),
                            outstanding_events_.end());
}

void MessagePumpUIStarboard::CancelDelayedLocked() {
  outstanding_events_lock_.AssertAcquired();
  for (SbEventIdSet::iterator it = outstanding_delayed_events_.begin();
       it != outstanding_delayed_events_.end(); ++it) {
    SbEventCancel(*it);
  }
  outstanding_delayed_events_.erase(outstanding_delayed_events_.begin(),
                                    outstanding_delayed_events_.end());
}

bool MessagePumpUIStarboard::RunOne(TimeTicks* out_delayed_work_time) {
  DCHECK(out_delayed_work_time);

  // We expect to start with a delegate, so we can DCHECK it, but any task we
  // run could call Quit and remove it.
  DCHECK(delegate_);
  if (!delegate_) {
#if !defined(COBALT_BUILD_TYPE_GOLD)
    // Abort if this is a QA build to signal that this is unexpected.
    CHECK(delegate_);
#endif
    // Drop the work if there is no delegate for it.
    return false;
  }

  // Do immediate work.
  Delegate::NextWorkInfo next_work_info = delegate_->DoWork();

  // If we did work, and we still have a delegate, return true, so we will be
  // called again.
  if (next_work_info.is_immediate()) {
    return !!delegate_;
  }

  // If the delegate has been removed, Quit() has been called, so no more work.
  if (!delegate_) {
    return false;
  }

  // No work was done, so only call back if there was idle work done, otherwise
  // go to sleep. ScheduleWork or ScheduleDelayedWork will be called if new work
  // is scheduled.
  return delegate_->DoIdleWork();
}

MessagePump::Delegate* MessagePumpForUI::SetDelegate(Delegate* delegate) {
  return std::exchange(delegate_, delegate);
}

}  // namespace base

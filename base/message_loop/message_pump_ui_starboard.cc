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

#include "build/build_config.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "starboard/event.h"
#include "starboard/system.h"

namespace base {

namespace {

void CallMessagePumpImmediate(void* context) {
  DCHECK(context);
  MessagePumpUIStarboard* pump =
      reinterpret_cast<MessagePumpUIStarboard*>(context);
  pump->CancelImmediate();
  pump->RunUntilIdle();
}

void CallMessagePumpDelayed(void* context) {
  DCHECK(context);
  MessagePumpUIStarboard* pump =
      reinterpret_cast<MessagePumpUIStarboard*>(context);
  pump->CancelDelayed();
  pump->RunUntilIdle();
}

}  // namespace

MessagePumpUIStarboard::MessagePumpUIStarboard() : delegate_(nullptr) {}

void MessagePumpUIStarboard::CancelDelayed() {
  base::AutoLock auto_lock(outstanding_events_lock_);
  CancelDelayedLocked();
}

void MessagePumpUIStarboard::CancelImmediate() {
  base::AutoLock auto_lock(outstanding_events_lock_);
  CancelImmediateLocked();
}

void MessagePumpUIStarboard::RunUntilIdle() {
  DCHECK(delegate_);
#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  // Abort if this is a QA build to signal that this is unexpected.
  CHECK(delegate_);
#endif

  if (should_quit())
    return;

  for (;;) {
    // Do some work and see if the next task is ready right away.
    Delegate::NextWorkInfo next_work_info = delegate_->DoWork();
    bool attempt_more_work = next_work_info.is_immediate();

    if (should_quit())
      break;

    if (attempt_more_work)
      continue;

    attempt_more_work = delegate_->DoIdleWork();

    if (should_quit())
      break;

    if (attempt_more_work)
      continue;

    // If there is delayed work.
    if (!next_work_info.delayed_run_time.is_max()) {
      ScheduleDelayedWork(next_work_info);
    }

    // Idle.
    break;
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

  run_loop_ = std::make_unique<RunLoop>();
  // Since the RunLoop was just created above, BeforeRun should be guaranteed to
  // return true (it only returns false if the RunLoop has been Quit already).
  CHECK(run_loop_->BeforeRun());
}

void MessagePumpUIStarboard::Quit() {
  delegate_ = nullptr;
  CancelAll();
  if (run_loop_) {
    run_loop_->AfterRun();
    run_loop_ = nullptr;
  }
}

void MessagePumpUIStarboard::ScheduleWork() {
  // Check if outstanding event already exists.
  if (outstanding_event_)
    return;

  base::AutoLock auto_lock(outstanding_events_lock_);
  outstanding_event_ =
      SbEventSchedule(&CallMessagePumpImmediate, this, 0);
}

void MessagePumpUIStarboard::ScheduleDelayedWork(
    const Delegate::NextWorkInfo& next_work_info) {
  if (next_work_info.is_immediate() || next_work_info.delayed_run_time.is_max()) {
    return;
  }

  TimeDelta delay = next_work_info.remaining_delay();
  if (delay.is_negative()) {
    delay = base::TimeDelta();
  }

  base::AutoLock auto_lock(outstanding_events_lock_);
  // Make sure any outstanding delayed event is canceled.
  CancelDelayedLocked();
  outstanding_delayed_event_ =
      SbEventSchedule(&CallMessagePumpDelayed, this, delay.InMicroseconds());
}

void MessagePumpUIStarboard::CancelAll() {
  base::AutoLock auto_lock(outstanding_events_lock_);
  CancelImmediateLocked();
  CancelDelayedLocked();
}

void MessagePumpUIStarboard::CancelImmediateLocked() {
  outstanding_events_lock_.AssertAcquired();
  if (!outstanding_event_)
    return;

  SbEventCancel(*outstanding_event_);
  outstanding_event_.reset();
}

void MessagePumpUIStarboard::CancelDelayedLocked() {
  outstanding_events_lock_.AssertAcquired();
  if (!outstanding_delayed_event_)
    return;

  SbEventCancel(*outstanding_delayed_event_);
  outstanding_delayed_event_.reset();
}

MessagePump::Delegate* MessagePumpForUI::SetDelegate(Delegate* delegate) {
  return std::exchange(delegate_, delegate);
}

}  // namespace base

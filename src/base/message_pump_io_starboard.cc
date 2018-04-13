// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/message_pump_io_starboard.h"

#include "base/auto_reset.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/posix/eintr_wrapper.h"
#include "base/time.h"
#include "nb/memory_scope.h"
#include "starboard/socket.h"
#include "starboard/socket_waiter.h"

namespace base {

MessagePumpIOStarboard::SocketWatcher::SocketWatcher()
    : interests_(kSbSocketWaiterInterestNone),
      socket_(kSbSocketInvalid),
      pump_(NULL),
      watcher_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {}

MessagePumpIOStarboard::SocketWatcher::~SocketWatcher() {
  if (SbSocketIsValid(socket_)) {
    StopWatchingSocket();
  }
}

bool MessagePumpIOStarboard::SocketWatcher::StopWatchingSocket() {
  SbSocket socket = Release();
  bool result = true;
  if (SbSocketIsValid(socket)) {
    DCHECK(pump_);
    result = pump_->StopWatching(socket);
  }
  pump_ = NULL;
  watcher_ = NULL;
  interests_ = kSbSocketWaiterInterestNone;
  return result;
}

void MessagePumpIOStarboard::SocketWatcher::Init(SbSocket socket,
                                                 bool persistent) {
  DCHECK(socket);
  DCHECK(!socket_);
  socket_ = socket;
  persistent_ = persistent;
}

SbSocket MessagePumpIOStarboard::SocketWatcher::Release() {
  SbSocket socket = socket_;
  socket_ = kSbSocketInvalid;
  return socket;
}

void MessagePumpIOStarboard::SocketWatcher::OnSocketReadyToRead(
    SbSocket socket,
    MessagePumpIOStarboard* pump) {
  if (!watcher_)
    return;
  pump->WillProcessIOEvent();
  watcher_->OnSocketReadyToRead(socket);
  pump->DidProcessIOEvent();
}

void MessagePumpIOStarboard::SocketWatcher::OnSocketReadyToWrite(
    SbSocket socket,
    MessagePumpIOStarboard* pump) {
  if (!watcher_)
    return;
  pump->WillProcessIOEvent();
  watcher_->OnSocketReadyToWrite(socket);
  pump->DidProcessIOEvent();
}

MessagePumpIOStarboard::MessagePumpIOStarboard()
    : keep_running_(true),
      in_run_(false),
      processed_io_events_(false),
      waiter_(SbSocketWaiterCreate()) {}

MessagePumpIOStarboard::~MessagePumpIOStarboard() {
  DCHECK(SbSocketWaiterIsValid(waiter_));
  SbSocketWaiterDestroy(waiter_);
}

bool MessagePumpIOStarboard::Watch(SbSocket socket,
                                   bool persistent,
                                   int mode,
                                   SocketWatcher* controller,
                                   Watcher* delegate) {
  DCHECK(SbSocketIsValid(socket));
  DCHECK(controller);
  DCHECK(delegate);
  DCHECK(mode == WATCH_READ || mode == WATCH_WRITE || mode == WATCH_READ_WRITE);
  // Watch should be called on the pump thread. It is not threadsafe, and your
  // watcher may never be registered.
  DCHECK(watch_socket_caller_checker_.CalledOnValidThread());

  int interests = kSbSocketWaiterInterestNone;
  if (mode & WATCH_READ) {
    interests |= kSbSocketWaiterInterestRead;
  }
  if (mode & WATCH_WRITE) {
    interests |= kSbSocketWaiterInterestWrite;
  }

  SbSocket old_socket = controller->Release();
  if (SbSocketIsValid(old_socket)) {
    // It's illegal to use this function to listen on 2 separate fds with the
    // same |controller|.
    if (old_socket != socket) {
      NOTREACHED() << "Sockets don't match" << old_socket << "!=" << socket;
      return false;
    }

    // Make sure we don't pick up any funky internal masks.
    int old_interest_mask =
        controller->interests() &
        (kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite);

    // Combine old/new event masks.
    interests |= old_interest_mask;

    // Must disarm the event before we can reuse it.
    SbSocketWaiterRemove(waiter_, old_socket);
  }

  // Set current interest mask and waiter for this event.
  bool result =
      SbSocketWaiterAdd(waiter_, socket, controller, OnSocketWaiterNotification,
                        interests, persistent);
  DCHECK(result);

  controller->Init(socket, persistent);
  controller->set_watcher(delegate);
  controller->set_pump(this);
  controller->set_interests(interests);

  return true;
}

bool MessagePumpIOStarboard::StopWatching(SbSocket socket) {
  return SbSocketWaiterRemove(waiter_, socket);
}

void MessagePumpIOStarboard::AddIOObserver(IOObserver* obs) {
  io_observers_.AddObserver(obs);
}

void MessagePumpIOStarboard::RemoveIOObserver(IOObserver* obs) {
  io_observers_.RemoveObserver(obs);
}

// Reentrant!
void MessagePumpIOStarboard::Run(Delegate* delegate) {
  TRACK_MEMORY_SCOPE("MessageLoop");
  DCHECK(keep_running_) << "Quit must have been called outside of Run!";
  AutoReset<bool> auto_reset_in_run(&in_run_, true);

  for (;;) {
    bool did_work = delegate->DoWork();
    if (!keep_running_)
      break;

    // NOTE: We need to have a wake-up pending any time there is work queued,
    // and the MessageLoop only wakes up the pump when the work queue goes from
    // 0 tasks to 1 task. If any work is scheduled on this MessageLoop (from
    // another thread) anywhere in between the call to DoWork() above and the
    // call to SbSocketWaiterWaitTimed() below, SbSocketWaiterWaitTimed() will
    // consume a wake-up, but leave the work queued. This will cause the
    // blocking wait further below to hang forever, no matter how many more
    // items are added to the queue. To resolve this, if this wait consumes a
    // wake-up, we set did_work to true so we will jump back to the top of the
    // loop and call delegate->DoWork() before we decide to block.

    SbSocketWaiterResult result = SbSocketWaiterWaitTimed(waiter_, 0);
    DCHECK_NE(kSbSocketWaiterResultInvalid, result);
    did_work |=
        (result == kSbSocketWaiterResultWokenUp) || processed_io_events_;
    processed_io_events_ = false;
    if (!keep_running_)
      break;

    // Let's play catchup on all delayed work before we loop.  This fixes bug
    // #5534709 by processing a large number of short delayed tasks quickly
    // before looping back to process non-delayed tasks (like paint).
    bool did_delayed_work = false;
    do {
      did_delayed_work = delegate->DoDelayedWork(&delayed_work_time_);
      did_work |= did_delayed_work;
    } while (did_delayed_work && keep_running_);

    if (!keep_running_)
      break;

    if (did_work)
      continue;

    did_work = delegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    if (delayed_work_time_.is_null()) {
      SbSocketWaiterWait(waiter_);
    } else {
      TimeDelta delay = delayed_work_time_ - TimeTicks::Now();
      if (delay > TimeDelta()) {
        SbSocketWaiterWaitTimed(waiter_, delay.ToSbTime());
      } else {
        // It looks like delayed_work_time_ indicates a time in the past, so we
        // need to call DoDelayedWork now.
        delayed_work_time_ = TimeTicks();
      }
    }
  }

  keep_running_ = true;
}

void MessagePumpIOStarboard::Quit() {
  DCHECK(in_run_);
  // Tell both the SbObjectWaiter and Run that they should break out of their
  // loops.
  keep_running_ = false;
  ScheduleWork();
}

void MessagePumpIOStarboard::ScheduleWork() {
  SbSocketWaiterWakeUp(waiter_);
}

void MessagePumpIOStarboard::ScheduleDelayedWork(
    const TimeTicks& delayed_work_time) {
  // We know that we can't be blocked on Wait right now since this method can
  // only be called on the same thread as Run, so we only need to update our
  // record of how long to sleep when we do sleep.
  delayed_work_time_ = delayed_work_time;
  ScheduleWork();
}

void MessagePumpIOStarboard::WillProcessIOEvent() {
  FOR_EACH_OBSERVER(IOObserver, io_observers_, WillProcessIOEvent());
}

void MessagePumpIOStarboard::DidProcessIOEvent() {
  FOR_EACH_OBSERVER(IOObserver, io_observers_, DidProcessIOEvent());
}

// static
void MessagePumpIOStarboard::OnSocketWaiterNotification(SbSocketWaiter waiter,
                                                        SbSocket socket,
                                                        void* context,
                                                        int ready_interests) {
  base::WeakPtr<SocketWatcher> controller =
      static_cast<SocketWatcher*>(context)->weak_factory_.GetWeakPtr();
  DCHECK(controller.get());

  MessagePumpIOStarboard* pump = controller->pump();
  pump->processed_io_events_ = true;

  // If not persistent, the watch has been released at this point.
  if (!controller->persistent()) {
    controller->Release();
  }

  if (ready_interests & kSbSocketWaiterInterestWrite) {
    controller->OnSocketReadyToWrite(socket, pump);
  }

  // Check |controller| in case it's been deleted previously.
  if (controller.get() && ready_interests & kSbSocketWaiterInterestRead) {
    controller->OnSocketReadyToRead(socket, pump);
  }
}

}  // namespace base

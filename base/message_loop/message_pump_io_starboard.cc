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

#include "base/message_loop/message_pump_io_starboard.h"

#include "base/auto_reset.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/posix/eintr_wrapper.h"
#include "base/time/time.h"
#include "starboard/common/socket.h"
#include "starboard/socket_waiter.h"

namespace base {

MessagePumpIOStarboard::SocketWatcher::SocketWatcher(const Location& from_here)
    : created_from_location_(from_here),
      interests_(kSbSocketWaiterInterestNone),
      socket_(kSbSocketInvalid),
      pump_(nullptr),
      watcher_(nullptr),
      weak_factory_(this) {}

MessagePumpIOStarboard::SocketWatcher::~SocketWatcher() {
  if (SbSocketIsValid(socket_)) {
    StopWatchingSocket();
  }
}

bool MessagePumpIOStarboard::SocketWatcher::UnregisterInterest(int interests) {
  bool result = true;
  if (SbSocketIsValid(socket_)) {
    // This may get called multiple times from TCPSocketStarboard.
    if (pump_) {
      result = pump_->UnregisterInterest(socket_, interests, this);
    } else
      Release();
  } else {
    interests_ = 0;
  }
  if (!interests_) {
    DCHECK(!SbSocketIsValid(socket_));
    pump_ = nullptr;
    watcher_ = nullptr;
  }
  return result;
}

bool MessagePumpIOStarboard::SocketWatcher::StopWatchingSocket() {
  return UnregisterInterest(kSbSocketWaiterInterestRead |
                            kSbSocketWaiterInterestWrite);
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
      processed_io_events_(false),
      waiter_(SbSocketWaiterCreate()) {}

MessagePumpIOStarboard::~MessagePumpIOStarboard() {
  DCHECK(SbSocketWaiterIsValid(waiter_));
  SbSocketWaiterDestroy(waiter_);
}

bool MessagePumpIOStarboard::UnregisterInterest(SbSocket socket,
                                                int dropped_interests,
                                                SocketWatcher* controller) {
  DCHECK(SbSocketIsValid(socket));
  DCHECK(controller);
  DCHECK(dropped_interests == kSbSocketWaiterInterestRead ||
         dropped_interests == kSbSocketWaiterInterestWrite ||
         dropped_interests ==
             (kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite));
  DCHECK_CALLED_ON_VALID_THREAD(watch_socket_caller_checker_);

  // Make sure we don't pick up any funky internal masks.
  int old_interest_mask =
      controller->interests() &
      (kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite);
  int interests = old_interest_mask & (~dropped_interests);
  if (interests == old_interest_mask) {
    // Interests didn't change, return.
    return true;
  }

  SbSocket old_socket = controller->Release();
  if (SbSocketIsValid(old_socket)) {
    // It's illegal to use this function to listen on 2 separate fds with the
    // same |controller|.
    if (old_socket != socket) {
      NOTREACHED() << "Sockets don't match " << old_socket << "!=" << socket;
      return false;
    }

    // Must disarm the event before we can reuse it.
    SbSocketWaiterRemove(waiter_, old_socket);
  } else {
    interests = kSbSocketWaiterInterestNone;
  }
  controller->set_interests(interests);

  if (!SbSocketIsValid(socket)) {
    NOTREACHED() << "Invalid socket " << socket;
    return false;
  }

  if (interests) {
    // Set current interest mask and waiter for this event.
    if (!SbSocketWaiterAdd(waiter_, socket, controller,
                           OnSocketWaiterNotification, interests,
                           controller->persistent())) {
      return false;
    }
    controller->Init(socket, controller->persistent());
  }
  return true;
}

bool MessagePumpIOStarboard::Watch(SbSocket socket,
                                   bool persistent,
                                   int interests,
                                   SocketWatcher* controller,
                                   Watcher* delegate) {
  DCHECK(SbSocketIsValid(socket));
  DCHECK(controller);
  DCHECK(delegate);
  DCHECK(interests == kSbSocketWaiterInterestRead ||
         interests == kSbSocketWaiterInterestWrite ||
         interests ==
             (kSbSocketWaiterInterestRead | kSbSocketWaiterInterestWrite));
  if ((controller->interests() & interests) == interests) {
    // Interests didn't change, return.
    return true;
  }
  // Watch should be called on the pump thread. It is not threadsafe, and your
  // watcher may never be registered.
  DCHECK_CALLED_ON_VALID_THREAD(watch_socket_caller_checker_);

  SbSocket old_socket = controller->Release();
  if (SbSocketIsValid(old_socket)) {
    // It's illegal to use this function to listen on 2 separate fds with the
    // same |controller|.
    if (old_socket != socket) {
      NOTREACHED() << "Sockets don't match " << old_socket << "!=" << socket;
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

  if (!SbSocketIsValid(socket)) {
    NOTREACHED() << "Invalid socket " << socket;
    return false;
  }

  // Set current interest mask and waiter for this event.
  if (!SbSocketWaiterAdd(waiter_, socket, controller,
                         OnSocketWaiterNotification, interests, persistent)) {
    return false;
  }

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
  AutoReset<bool> auto_reset_keep_running(&keep_running_, true);

  for (;;) {
    Delegate::NextWorkInfo next_work_info = delegate->DoWork();
    bool immediate_work_available = next_work_info.is_immediate();

    if (should_quit())
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

    bool attempt_more_work =
        (result == kSbSocketWaiterResultWokenUp) || immediate_work_available || processed_io_events_;
    processed_io_events_ = false;

    if (should_quit())
      break;

    if (attempt_more_work)
      continue;

    attempt_more_work = delegate->DoIdleWork();

    if (should_quit())
      break;

    if (attempt_more_work)
      continue;

    if (next_work_info.delayed_run_time.is_max()) {
      SbSocketWaiterWait(waiter_);
    } else {
      SbSocketWaiterWaitTimed(waiter_, next_work_info.remaining_delay().InMicroseconds());
    }

    if (should_quit())
      break;
  }
}

void MessagePumpIOStarboard::Quit() {
  // Tell both the SbObjectWaiter and Run that they should break out of their
  // loops.
  keep_running_ = false;
  ScheduleWork();
}

void MessagePumpIOStarboard::ScheduleWork() {
  SbSocketWaiterWakeUp(waiter_);
}

void MessagePumpIOStarboard::ScheduleDelayedWork(
    const Delegate::NextWorkInfo& next_work_info) {
  // We know that we can't be blocked on Wait right now since this method can
  // only be called on the same thread as Run, so we only need to update our
  // record of how long to sleep when we do sleep.
}

void MessagePumpIOStarboard::WillProcessIOEvent() {
  for (IOObserver& observer : io_observers_) {
    observer.WillProcessIOEvent();
  }
}

void MessagePumpIOStarboard::DidProcessIOEvent() {
  for (IOObserver& observer : io_observers_) {
    observer.DidProcessIOEvent();
  }
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

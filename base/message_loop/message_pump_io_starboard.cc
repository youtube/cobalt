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

#if SB_API_VERSION >= 16
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#endif

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
#if SB_API_VERSION >= 16
      socket_(-1),
#else
      socket_(kSbSocketInvalid),
#endif
      pump_(nullptr),
      watcher_(nullptr),
      weak_factory_(this) {}

MessagePumpIOStarboard::SocketWatcher::~SocketWatcher() {
#if SB_API_VERSION >= 16
  if (socket_ >= 0) {
    StopWatchingFileDescriptor();
  }
#else
  if (SbSocketIsValid(socket_)) {
    StopWatchingSocket();
  }
#endif
}

#if SB_API_VERSION >= 16
bool MessagePumpIOStarboard::SocketWatcher::StopWatchingFileDescriptor() {
  watcher_ = nullptr;
  interests_ = kSbSocketWaiterInterestNone;
  if (socket_ < 0) {
    pump_ = nullptr;
    // If this watcher is not watching anything, no-op and return success.
    return true;
  }
  int socket = Release();
  bool result = true;
  if (socket >= 0) {
    DCHECK(pump_);
    result = pump_->StopWatchingFileDescriptor(socket);
  }
  pump_ = nullptr;
  return result;
}
#else
bool MessagePumpIOStarboard::SocketWatcher::StopWatchingSocket() {
  watcher_ = nullptr;
  interests_ = kSbSocketWaiterInterestNone;
  if (!SbSocketIsValid(socket_)) {
    pump_ = nullptr;
    // If this watcher is not watching anything, no-op and return success.
    return true;
  }

  SbSocket socket = Release();
  bool result = true;
  if (SbSocketIsValid(socket)) {
    DCHECK(pump_);
    result = pump_->StopWatching(socket);
  }
  pump_ = nullptr;
  return result;
}
#endif

#if SB_API_VERSION >= 16
void MessagePumpIOStarboard::SocketWatcher::Init(int socket,
                                                 bool persistent) {
  DCHECK(socket >= 0);
  DCHECK(socket_ < 0);
#else
void MessagePumpIOStarboard::SocketWatcher::Init(SbSocket socket,
                                                 bool persistent) {
  DCHECK(socket);
  DCHECK(!socket_);
#endif
  socket_ = socket;
  persistent_ = persistent;
}

#if SB_API_VERSION >= 16
int MessagePumpIOStarboard::SocketWatcher::Release() {
  int socket = socket_;
  socket_ = -1;
  return socket;
}
#else
SbSocket MessagePumpIOStarboard::SocketWatcher::Release() {
  SbSocket socket = socket_;
  socket_ = kSbSocketInvalid;
  return socket;
}
#endif

#if SB_API_VERSION >= 16
void MessagePumpIOStarboard::SocketWatcher::OnFileCanReadWithoutBlocking(
    int socket,
    MessagePumpIOStarboard* pump) {
  if (!watcher_)
    return;
  pump->WillProcessIOEvent();
  watcher_->OnFileCanReadWithoutBlocking(socket);
  pump->DidProcessIOEvent();
}

void MessagePumpIOStarboard::SocketWatcher::OnFileCanWriteWithoutBlocking(
    int socket,
    MessagePumpIOStarboard* pump) {
  if (!watcher_)
    return;
  pump->WillProcessIOEvent();
  watcher_->OnFileCanWriteWithoutBlocking(socket);
  pump->DidProcessIOEvent();
}

#else
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
#endif

MessagePumpIOStarboard::MessagePumpIOStarboard()
    : keep_running_(true),
      processed_io_events_(false),
      waiter_(SbSocketWaiterCreate()) {}

MessagePumpIOStarboard::~MessagePumpIOStarboard() {
  DCHECK(SbSocketWaiterIsValid(waiter_));
  SbSocketWaiterDestroy(waiter_);
}

#if SB_API_VERSION >= 16
bool MessagePumpIOStarboard::WatchFileDescriptor(int socket,
                                   bool persistent,
                                   int mode,
                                   SocketWatcher* controller,
                                   Watcher* delegate) {
  DCHECK(socket >= 0);
#else
bool MessagePumpIOStarboard::Watch(SbSocket socket,
                                   bool persistent,
                                   int mode,
                                   SocketWatcher* controller,
                                   Watcher* delegate) {
  DCHECK(SbSocketIsValid(socket));
#endif
  DCHECK(controller);
  DCHECK(delegate);
  DCHECK(mode == WATCH_READ || mode == WATCH_WRITE || mode == WATCH_READ_WRITE);
  // Watch should be called on the pump thread. It is not threadsafe, and your
  // watcher may never be registered.
  DCHECK_CALLED_ON_VALID_THREAD(watch_socket_caller_checker_);

  int interests = kSbSocketWaiterInterestNone;
  if (mode & WATCH_READ) {
    interests |= kSbSocketWaiterInterestRead;
  }
  if (mode & WATCH_WRITE) {
    interests |= kSbSocketWaiterInterestWrite;
  }

#if SB_API_VERSION >= 16
  int old_socket = controller->Release();
  if (old_socket >= 0) {
#else
  SbSocket old_socket = controller->Release();
  if (SbSocketIsValid(old_socket)) {
#endif
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
#if SB_API_VERSION >= 16
    SbPosixSocketWaiterRemove(waiter_, old_socket);
#else
    SbSocketWaiterRemove(waiter_, old_socket);
#endif  // SB_API_VERSION >= 16
  }

  // Set current interest mask and waiter for this event.
  bool result = false;
#if SB_API_VERSION >= 16
  result = SbPosixSocketWaiterAdd(waiter_, socket, controller,
                             OnPosixSocketWaiterNotification, interests, persistent);

#else
  result = SbSocketWaiterAdd(waiter_, socket, controller,
                         OnSocketWaiterNotification, interests, persistent);
#endif  // SB_API_VERSION >= 16
  if (result == false) {
    return false;
  }

  controller->Init(socket, persistent);
  controller->set_watcher(delegate);
  controller->set_pump(this);
  controller->set_interests(interests);
  return true;
}

#if SB_API_VERSION >= 16
bool MessagePumpIOStarboard::StopWatchingFileDescriptor(int socket) {
    return SbPosixSocketWaiterRemove(waiter_, socket);
}
#else
bool MessagePumpIOStarboard::StopWatching(SbSocket socket) {
  return SbSocketWaiterRemove(waiter_, socket);
}
#endif  // SB_API_VERSION >= 16 || SB_IS(MODULAR)

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

#if SB_API_VERSION >= 16

// static
void MessagePumpIOStarboard::OnPosixSocketWaiterNotification(SbSocketWaiter waiter,
                                                             int socket,
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
    controller->OnFileCanWriteWithoutBlocking(socket, pump);
  }

  // Check |controller| in case it's been deleted previously.
  if (controller.get() && ready_interests & kSbSocketWaiterInterestRead) {
    controller->OnFileCanReadWithoutBlocking(socket, pump);
  }
}

#else
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

#endif  // SB_API_VERSION >= 16
}  // namespace base

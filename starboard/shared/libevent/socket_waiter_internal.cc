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

#include "starboard/shared/libevent/socket_waiter_internal.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <unistd.h>

#include <map>
#include <utility>

#include "starboard/log.h"
#include "starboard/shared/posix/handle_eintr.h"
#include "starboard/shared/posix/socket_internal.h"
#include "starboard/shared/posix/time_internal.h"
#include "starboard/socket_waiter.h"
#include "starboard/thread.h"
#include "third_party/libevent/event.h"

namespace {
int SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    flags = 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// We do this because it's our style to use explicitly-sized ints when not just
// using int, but libevent uses shorts explicitly in its interface.
SB_COMPILE_ASSERT(sizeof(int16_t) == sizeof(short),  // NOLINT[runtime/int]
                  Short_is_not_int16);
}  // namespace

SbSocketWaiterPrivate::SbSocketWaiterPrivate()
    : thread_(SbThreadGetCurrent()),
      base_(event_base_new()),
      waiting_(false),
      woken_up_(false) {
  int fds[2];
  int result = pipe(fds);
  SB_DCHECK(result == 0);

  wakeup_read_fd_ = fds[0];
  result = SetNonBlocking(wakeup_read_fd_);
  SB_DCHECK(result == 0);

  wakeup_write_fd_ = fds[1];
  result = SetNonBlocking(wakeup_write_fd_);
  SB_DCHECK(result == 0);

  event_set(&wakeup_event_, wakeup_read_fd_, EV_READ | EV_PERSIST,
            &SbSocketWaiterPrivate::LibeventWakeUpCallback, this);
  event_base_set(base_, &wakeup_event_);
  event_add(&wakeup_event_, NULL);
}

SbSocketWaiterPrivate::~SbSocketWaiterPrivate() {
  WaiteesMap::iterator it = waitees_.begin();
  while (it != waitees_.end()) {
    Waitee* waitee = it->second;
    ++it;  // Increment before removal.
    Remove(waitee->socket);
  }

  close(wakeup_read_fd_);
  close(wakeup_write_fd_);

  event_del(&wakeup_event_);
  event_base_free(base_);
}

bool SbSocketWaiterPrivate::Add(SbSocket socket,
                                void* context,
                                SbSocketWaiterCallback callback,
                                int interests,
                                bool persistent) {
  SB_DCHECK(SbThreadIsCurrent(thread_));

  if (!SbSocketIsValid(socket)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Socket (" << socket << ") is invalid.";
    return false;
  }

  if (!interests) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": No interests provided.";
    return false;
  }

  // The policy is not to add a socket to a waiter if it is registered with
  // another waiter.

  // TODO(***REMOVED***): If anyone were to want to add a socket to a different waiter,
  // it would probably be another thread, so doing this check without locking is
  // probably wrong. But, it is also a pain, and, at this precise moment, socket
  // access is all going to come from one I/O thread anyway, and there will only
  // be one waiter.
  if (SbSocketWaiterIsValid(socket->waiter)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Socket already has waiter.";
    return false;
  }

  Waitee* waitee =
      new Waitee(this, socket, context, callback, interests, persistent);
  AddWaitee(waitee);
  int16_t events = 0;
  if (interests & kSbSocketWaiterInterestRead) {
    events |= EV_READ;
  }

  if (interests & kSbSocketWaiterInterestWrite) {
    events |= EV_WRITE;
  }

  if (persistent) {
    events |= EV_PERSIST;
  }

  event_set(&waitee->event, socket->socket_fd, events,
            &SbSocketWaiterPrivate::LibeventSocketCallback, waitee);
  event_base_set(base_, &waitee->event);
  socket->waiter = this;
  event_add(&waitee->event, NULL);
  return true;
}

bool SbSocketWaiterPrivate::Remove(SbSocket socket) {
  SB_DCHECK(SbThreadIsCurrent(thread_));
  if (!SbSocketIsValid(socket)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Socket (" << socket << ") is invalid.";
    return false;
  }

  if (socket->waiter != this) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Socket (" << socket << ") "
                   << "is watched by Waiter (" << socket->waiter << "), "
                   << "not this Waiter (" << this << ").";
    SB_DSTACK(ERROR);
    return false;
  }

  Waitee* waitee = RemoveWaitee(socket);
  if (!waitee) {
    return false;
  }

  event_del(&waitee->event);
  socket->waiter = kSbSocketWaiterInvalid;

  delete waitee;
  return true;
}

void SbSocketWaiterPrivate::Wait() {
  SB_DCHECK(SbThreadIsCurrent(thread_));

  // We basically wait for the largest amount of time to achieve an indefinite
  // block.
  WaitTimed(kSbTimeMax);
}

SbSocketWaiterResult SbSocketWaiterPrivate::WaitTimed(SbTime duration) {
  SB_DCHECK(SbThreadIsCurrent(thread_));

  // The way to do this is apparently to create a timeout event, call WakeUp
  // inside that callback, and then just do a normal wait.
  struct event event;
  timeout_set(&event, &SbSocketWaiterPrivate::LibeventTimeoutCallback, this);
  event_base_set(base_, &event);

  struct timeval tv;
  ToTimevalDuration(duration, &tv);
  timeout_add(&event, &tv);

  waiting_ = true;
  event_base_loop(base_, 0);
  waiting_ = false;

  SbSocketWaiterResult result =
      woken_up_ ? kSbSocketWaiterResultWokenUp : kSbSocketWaiterResultTimedOut;
  woken_up_ = false;

  // We clean this up, in case we were bewakened early, to prevent a suprious
  // wake-up later.
  timeout_del(&event);

  return result;
}

void SbSocketWaiterPrivate::WakeUp(bool timeout) {
  // We may be calling from a separate thread, so we have to be clever. The
  // version of libevent we are using (14.x) does not really do thread-safety,
  // despite the documentation that says otherwise. But, sending a byte through
  // a local pipe gets the job done safely.
  char buf = timeout ? 0 : 1;
  int bytes_written = HANDLE_EINTR(write(wakeup_write_fd_, &buf, 1));
  SB_DCHECK(bytes_written == 1 || errno == EAGAIN)
      << "[bytes_written:" << bytes_written << "] [errno:" << errno << "]";
}

// static
void SbSocketWaiterPrivate::LibeventSocketCallback(int /*fd*/,
                                                   int16_t event,
                                                   void* context) {
  Waitee* waitee = reinterpret_cast<Waitee*>(context);
  waitee->waiter->HandleSignal(waitee, event);
}

// static
void SbSocketWaiterPrivate::LibeventTimeoutCallback(int /*fd*/,
                                                    int16_t /*event*/,
                                                    void* context) {
  reinterpret_cast<SbSocketWaiter>(context)->WakeUp(true);
}

// static
void SbSocketWaiterPrivate::LibeventWakeUpCallback(int /*fd*/,
                                                   int16_t /*event*/,
                                                   void* context) {
  reinterpret_cast<SbSocketWaiter>(context)->HandleWakeUpRead();
}

void SbSocketWaiterPrivate::HandleSignal(Waitee* waitee,
                                         short events) {  // NOLINT[runtime/int]
  int interests = 0;
  if (events & EV_READ) {
    interests |= kSbSocketWaiterInterestRead;
  }

  if (events & EV_WRITE) {
    interests |= kSbSocketWaiterInterestWrite;
  }

  // Remove the non-persistent waitee before calling the callback, so that we
  // can add another waitee in the callback if we need to. This is also why we
  // copy all the fields we need out of waitee.
  SbSocket socket = waitee->socket;
  void* context = waitee->context;
  SbSocketWaiterCallback callback = waitee->callback;
  if (!waitee->persistent) {
    Remove(waitee->socket);
  }

  callback(this, socket, context, interests);
}

void SbSocketWaiterPrivate::HandleWakeUpRead() {
  SB_DCHECK(waiting_);
  // Remove and discard the wakeup byte.
  char buf;
  int bytes_read = HANDLE_EINTR(read(wakeup_read_fd_, &buf, 1));
  SB_DCHECK(bytes_read == 1);
  if (buf != 0) {
    woken_up_ = true;
  }
  event_base_loopbreak(base_);
}

void SbSocketWaiterPrivate::AddWaitee(Waitee* waitee) {
  waitees_.insert(std::make_pair(waitee->socket, waitee));
}

SbSocketWaiterPrivate::Waitee* SbSocketWaiterPrivate::GetWaitee(
    SbSocket socket) {
  WaiteesMap::iterator it = waitees_.find(socket);
  if (it == waitees_.end()) {
    return NULL;
  }

  return it->second;
}

SbSocketWaiterPrivate::Waitee* SbSocketWaiterPrivate::RemoveWaitee(
    SbSocket socket) {
  WaiteesMap::iterator it = waitees_.find(socket);
  if (it == waitees_.end()) {
    return NULL;
  }

  Waitee* result = it->second;
  waitees_.erase(it);
  return result;
}

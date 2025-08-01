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

#include "starboard/shared/starboard/queue_application.h"

#include <atomic>
#include <limits>

#include "starboard/common/condition_variable.h"
#include "starboard/common/log.h"
#include "starboard/common/time.h"
#include "starboard/event.h"

namespace starboard::shared::starboard {

void QueueApplication::Wake() {
  if (IsCurrentThread()) {
    return;
  }

  if (!MayHaveSystemEvents()) {
    event_queue_.Wake();
    return;
  } else {
    WakeSystemEventWait();
  }
}

Application::Event* QueueApplication::GetNextEvent() {
  if (!MayHaveSystemEvents()) {
    return GetNextInjectedEvent();
  }

  // The construction of this loop is somewhat deliberate. The main UI message
  // pump will inject an event every time it needs to do deferred work. If we
  // don't prioritize system window events, they can get starved by a constant
  // stream of work.
  Event* event = NULL;
  while (!event) {
    event = PollNextSystemEvent();
    if (event) {
      continue;
    }

    // Then poll the generic queue.
    event = PollNextInjectedEvent();
    if (event) {
      continue;
    }

    // Then we block indefinitely on the Window's DFB queue.
    event = WaitForSystemEventWithTimeout(GetNextTimedEventTargetTime() -
                                          CurrentMonotonicTime());
  }

  return event;
}

void QueueApplication::Inject(Event* event) {
  event_queue_.Put(event);
  if (MayHaveSystemEvents()) {
    WakeSystemEventWait();
  }
}

void QueueApplication::InjectTimedEvent(TimedEvent* timed_event) {
  if (timed_event_queue_.Inject(timed_event)) {
    // The time to wake up has moved earlier, so wake up the event queue to
    // recalculate the wait.
    Wake();
  }
}

void QueueApplication::CancelTimedEvent(SbEventId event_id) {
  timed_event_queue_.Cancel(event_id);

  // The wait duration will only get longer after cancelling an event, so the
  // waiter will wake up as previously scheduled, see there is nothing to do,
  // and go back to sleep.
}

void QueueApplication::InjectAndProcess(SbEventType type,
                                        bool checkSystemEvents) {
  std::atomic_bool event_processed{false};
  Event* flagged_event = new Event(
      type, const_cast<std::atomic_bool*>(&event_processed), [](void* flag) {
        auto* bool_flag =
            const_cast<std::atomic_bool*>(static_cast<std::atomic_bool*>(flag));
        bool_flag->store(true);
      });
  Inject(flagged_event);
  while (!event_processed.load()) {
    if (checkSystemEvents) {
      DispatchAndDelete(GetNextEvent());
    } else {
      DispatchAndDelete(GetNextInjectedEvent());
    }
  }
}

Application::TimedEvent* QueueApplication::GetNextDueTimedEvent() {
  return timed_event_queue_.Get();
}

int64_t QueueApplication::GetNextTimedEventTargetTime() {
  return timed_event_queue_.GetTime();
}

QueueApplication::TimedEventQueue::TimedEventQueue() : set_(&IsLess) {}

QueueApplication::TimedEventQueue::~TimedEventQueue() {
  std::lock_guard lock(mutex_);
  for (TimedEventMap::iterator i = map_.begin(); i != map_.end(); ++i) {
    delete i->second;
  }
  map_.clear();
  set_.clear();
}

bool QueueApplication::TimedEventQueue::Inject(TimedEvent* timed_event) {
  std::lock_guard lock(mutex_);
  int64_t oldTime = GetTimeLocked();
  map_[timed_event->id] = timed_event;
  set_.insert(timed_event);
  return (timed_event->target_time < oldTime);
}

void QueueApplication::TimedEventQueue::Cancel(SbEventId event_id) {
  std::lock_guard lock(mutex_);
  TimedEventMap::iterator i = map_.find(event_id);
  if (i == map_.end()) {
    return;
  }

  TimedEvent* timed_event = i->second;
  map_.erase(i);
  set_.erase(timed_event);
  delete timed_event;
}

Application::TimedEvent* QueueApplication::TimedEventQueue::Get() {
  std::lock_guard lock(mutex_);
  if (set_.empty()) {
    return NULL;
  }

  TimedEvent* timed_event = *(set_.begin());
  if (timed_event->target_time > CurrentMonotonicTime()) {
    return NULL;
  }

  map_.erase(timed_event->id);
  set_.erase(timed_event);
  return timed_event;
}

int64_t QueueApplication::TimedEventQueue::GetTime() {
  std::lock_guard lock(mutex_);
  return GetTimeLocked();
}

int64_t QueueApplication::TimedEventQueue::GetTimeLocked() {
  if (set_.empty()) {
    return std::numeric_limits<int64_t>::max();
  }

  TimedEvent* timed_event = *(set_.begin());
  int64_t time = timed_event->target_time;
  int64_t now = CurrentMonotonicTime();
  if (time < now) {
    return now;
  }

  return time;
}

// static
bool QueueApplication::TimedEventQueue::IsLess(const TimedEvent* lhs,
                                               const TimedEvent* rhs) {
  int64_t time_difference = lhs->target_time - rhs->target_time;
  if (time_difference != 0) {
    return time_difference < 0;
  }

  // If the time differences are the same, ensure there is a strict and stable
  // ordering.
  return reinterpret_cast<uintptr_t>(lhs) < reinterpret_cast<uintptr_t>(rhs);
}

Application::Event* QueueApplication::PollNextInjectedEvent() {
  Event* event = event_queue_.Poll();
  if (event != NULL) {
    return event;
  }

  TimedEvent* timed_event = GetNextDueTimedEvent();
  if (timed_event != NULL) {
    return new Event(timed_event);
  }

  return NULL;
}

Application::Event* QueueApplication::GetNextInjectedEvent() {
  for (;;) {
    int64_t delay = GetNextTimedEventTargetTime() - CurrentMonotonicTime();
    Event* event = event_queue_.GetTimed(delay);
    if (event != NULL) {
      return event;
    }

    TimedEvent* timed_event = GetNextDueTimedEvent();
    if (timed_event != NULL) {
      return new Event(timed_event);
    }
  }
}

}  // namespace starboard::shared::starboard

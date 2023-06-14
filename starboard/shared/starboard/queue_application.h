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

// A simple queue-based application implementation.

#ifndef STARBOARD_SHARED_STARBOARD_QUEUE_APPLICATION_H_
#define STARBOARD_SHARED_STARBOARD_QUEUE_APPLICATION_H_

#include <map>
#include <set>

#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/common/queue.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {

// An application implementation that uses a signaling thread-safe queue to
// manage event dispatching.
class QueueApplication : public Application {
 public:
#if SB_API_VERSION >= 15
  explicit QueueApplication(SbEventHandleCallback sb_event_handle_callback)
      : Application(sb_event_handle_callback) {}
#else
  QueueApplication() {}
#endif  // SB_API_VERSION >= 15
  ~QueueApplication() override {}

 protected:
  // Wakes up GetNextEvent, and ensures it recalculates the wait duration.
  void Wake();

  // --- Application overrides ---
  Event* GetNextEvent() override;
  void Inject(Event* event) override;
  void InjectTimedEvent(TimedEvent* timed_event) override;
  void CancelTimedEvent(SbEventId event_id) override;
  TimedEvent* GetNextDueTimedEvent() override;
  SbTimeMonotonic GetNextTimedEventTargetTime() override;

  // Add the given event onto the event queue, then process the queue until the
  // event is handled. This is similar to DispatchAndDelete but will process
  // the queue in order until the new event is handled rather than processing
  // the event out of order. If the caller is part of system event handling,
  // then consider passing |checkSystemEvents| = false to avoid recursion if
  // needed.
  void InjectAndProcess(SbEventType type, bool checkSystemEvents);

  // Returns true if it is valid to poll/query for system events.
  virtual bool MayHaveSystemEvents() = 0;

  // Returns an event if one exists, otherwise returns NULL.
  virtual Event* PollNextSystemEvent() {
    return WaitForSystemEventWithTimeout(SbTime());
  }

  // Waits for an event until the timeout |time| runs out.  If an event occurs
  // in this time, it is returned, otherwise NULL is returned. If |time| is zero
  // or negative, then this should function effectively like a no-wait poll.
  virtual Event* WaitForSystemEventWithTimeout(SbTime time) = 0;

  // Wakes up any thread waiting within a call to
  // WaitForSystemEventWithTimeout().
  virtual void WakeSystemEventWait() = 0;

 private:
#if SB_API_VERSION >= 14
  // Use Inject() or InjectAndProcess(). DispatchAndDelete() ignores the event
  // queue and processes the event out of order which can lead to bugs.
  using Application::DispatchAndDelete;
#endif

  // Specialization of Queue for starboard events.  It differs in that it has
  // the responsibility of deleting heap allocated starboard events in its
  // destructor.  Note the non-virtual destructor, which is intentional and
  // safe, as Queue has no virtual functions and EventQueue is never used
  // polymorphically.
  class EventQueue : public Queue<Event*> {
   public:
    ~EventQueue() {
      while (Event* event = Poll()) {
        delete event;
      }
    }
  };

  class TimedEventQueue {
   public:
    TimedEventQueue();
    ~TimedEventQueue();

    // Returns whether the new event pushed up the next wakeup time.
    bool Inject(TimedEvent* timed_event);
    void Cancel(SbEventId event_id);
    TimedEvent* Get();
    SbTimeMonotonic GetTime();

   private:
    SbTimeMonotonic GetTimeLocked();
    typedef bool (*TimedEventComparator)(const TimedEvent* lhs,
                                         const TimedEvent* rhs);
    static bool IsLess(const TimedEvent* lhs, const TimedEvent* rhs);

    Mutex mutex_;
    typedef std::map<SbEventId, TimedEvent*> TimedEventMap;
    TimedEventMap map_;
    typedef std::set<TimedEvent*, TimedEventComparator> TimedEventSet;
    TimedEventSet set_;
  };

  // Polls the queue for the next event, returning NULL if there is nothing to
  // execute.
  Event* PollNextInjectedEvent();

  // Returns the next non-system event, waiting for it if none are currently
  // available.  Called from within GetNextEvent().
  Event* GetNextInjectedEvent();

  TimedEventQueue timed_event_queue_;

  // The queue of events that have not yet been dispatched.
  EventQueue event_queue_;
};

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_QUEUE_APPLICATION_H_

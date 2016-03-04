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

// A simple queue-based application implementation.

#ifndef STARBOARD_SHARED_STARBOARD_QUEUE_APPLICATION_H_
#define STARBOARD_SHARED_STARBOARD_QUEUE_APPLICATION_H_

#include <map>
#include <set>

#include "starboard/condition_variable.h"
#include "starboard/mutex.h"
#include "starboard/queue.h"
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
  QueueApplication() {}
  ~QueueApplication() SB_OVERRIDE {}

 protected:
  // Polls the queue for the next event, returning NULL if there is nothing to
  // execute.
  Event* PollNextEvent();

  // Wakes up GetNextEvent, and ensures it recalculates the wait duration.
  virtual void Wake();

  // --- Application overrides ---
  Event* GetNextEvent() SB_OVERRIDE;
  void Inject(Event* event) SB_OVERRIDE;
  void InjectTimedEvent(TimedEvent* timed_event) SB_OVERRIDE;
  void CancelTimedEvent(SbEventId event_id) SB_OVERRIDE;
  TimedEvent* GetNextDueTimedEvent() SB_OVERRIDE;
  SbTimeMonotonic GetNextTimedEventTargetTime() SB_OVERRIDE;

 private:
  class TimedEventQueue {
   public:
    TimedEventQueue();

    // Returns whether the new event pushed up the next wakeup time.
    bool Inject(TimedEvent* timed_event);
    void Cancel(SbEventId event_id);
    TimedEvent* Get();
    SbTimeMonotonic GetTime();

   private:
    SbTimeMonotonic GetTimeLocked();
    typedef bool (*TimedEventComparator)(TimedEvent* lhs, TimedEvent* rhs);
    static bool Compare(TimedEvent* lhs, TimedEvent* rhs);

    Mutex mutex_;
    typedef std::map<SbEventId, TimedEvent*> TimedEventMap;
    TimedEventMap map_;
    typedef std::set<TimedEvent*, TimedEventComparator> TimedEventSet;
    TimedEventSet set_;
  };

  TimedEventQueue timed_event_queue_;

  // The queue of events that have not yet been dispatched.
  Queue<Event*> event_queue_;
};

}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_QUEUE_APPLICATION_H_

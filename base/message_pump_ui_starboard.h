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

#ifndef BASE_MESSAGE_PUMP_UI_STARBOARD_H_
#define BASE_MESSAGE_PUMP_UI_STARBOARD_H_

#include <set>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_pump.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "starboard/event.h"

namespace base {

// MessagePump that integrates with the Starboard message pump. Starboard has
// its own main loop, so the MessagePumpUIStarboard just piggybacks on that
// rather than implementing its own pump.
//
// To adopt Starboard's message pump, one simply has to create a MessageLoop of
// TYPE_UI from the Starboard message pump thread. A traditional place to do
// this would be in the kSbEventStart event handler. One has to be sure to keep
// the MessageLoop alive for as long as the application wants to use the
// Starboard message pump as a MessageLoop. That would typically be for the
// entire lifetime of the application, and in that case, the MessageLoop would
// traditionally be deleted in the kSbEventStop handler.
class BASE_EXPORT MessagePumpUIStarboard : public MessagePump {
 public:
  MessagePumpUIStarboard();

  // Runs one iteration of the run loop, and reschedules another call, if
  // necessary.
  void RunOneAndReschedule(bool delayed);

  // --- MessagePump Implementation ---

  virtual void Run(Delegate* delegate) override;
  virtual void Quit() override;
  virtual void ScheduleWork() override;
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time) override;
  virtual void Start(Delegate* delegate);

 protected:
  virtual ~MessagePumpUIStarboard() {}

 private:
  // Cancels all outstanding scheduled callback events, if any.
  void CancelAll();

  // Cancels immediate schedule callback events.
  void CancelImmediate();

  // Cancels delayed schedule callback events.
  void CancelDelayed();

  // Cancel workhorse that assumes |outstanding_events_lock_| is locked.
  void CancelImmediateLocked();

  // Cancel delayed workhorse that assumes |outstanding_events_lock_| is locked.
  void CancelDelayedLocked();

  // Runs one iteration of the run loop, returning whether to schedule another
  // iteration or not. Places the delay, if any, in |out_delayed_work_time|.
  bool RunOne(base::TimeTicks* out_delayed_work_time);

  // The top-level RunLoop for the MessageLoopForUI. A MessageLoop needs a
  // top-level RunLoop to be considered "running."
  scoped_ptr<base::RunLoop> run_loop_;

  // The MessagePump::Delegate configured in Start().
  Delegate* delegate_;

  // Lock protecting outstanding scheduled callback events.
  base::Lock outstanding_events_lock_;

  // A set of scheduled callback event IDs.
  typedef std::set<SbEventId> SbEventIdSet;

  // The set of outstanding scheduled callback events for immediate work.
  SbEventIdSet outstanding_events_;

  // The set of outstanding scheduled callback events for delayed work.
  SbEventIdSet outstanding_delayed_events_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpUIStarboard);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_UI_STARBOARD_H_

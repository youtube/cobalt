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

#ifndef BASE_MESSAGE_PUMP_UI_STARBOARD_H_
#define BASE_MESSAGE_PUMP_UI_STARBOARD_H_

#include <set>

#include "base/base_export.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/watchable_io_message_pump_posix.h"
#include "base/run_loop.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "starboard/event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
class BASE_EXPORT MessagePumpUIStarboard : public MessagePump, public WatchableIOMessagePumpPosix {
 public:

  MessagePumpUIStarboard();
  virtual ~MessagePumpUIStarboard() { Quit(); }

  MessagePumpUIStarboard(const MessagePumpUIStarboard&) = delete;
  MessagePumpUIStarboard& operator=(const MessagePumpUIStarboard&) = delete;

  // Cancels delayed schedule callback events.
  void CancelDelayed();

  // Cancels immediate schedule callback events.
  void CancelImmediate();

  // Runs one iteration of the run loop, and schedules another iteration if
  // necessary.
  void RunUntilIdle();

  // --- MessagePump Implementation ---

  virtual void Run(Delegate* delegate) override;
  virtual void Quit() override;
  virtual void ScheduleWork() override;
  virtual void ScheduleDelayedWork(
      const Delegate::NextWorkInfo& next_work_info) override;

  // Attaches |delegate| to this native MessagePump. |delegate| will from then
  // on be invoked by the native loop to process application tasks.
  virtual void Attach(Delegate* delegate);

 protected:
  Delegate* SetDelegate(Delegate* delegate);

 private:
  // Cancels all outstanding scheduled callback events, if any.
  void CancelAll();

  // Cancel workhorse that assumes |outstanding_events_lock_| is locked.
  void CancelImmediateLocked();

  // Cancel delayed workhorse that assumes |outstanding_events_lock_| is locked.
  void CancelDelayedLocked();

  // If the delegate has been removed, Quit() has been called.
  bool should_quit() const { return delegate_ == nullptr; }

  // Maintain a RunLoop attached to the starboard thread.
  std::unique_ptr<RunLoop> run_loop_;

  // The MessagePump::Delegate configured in Start().
  Delegate* delegate_;

  // Lock protecting outstanding scheduled callback events.
  base::Lock outstanding_events_lock_;

  // The set of outstanding scheduled callback events for immediate work.
  absl::optional<SbEventId> outstanding_event_;

  // The set of outstanding scheduled callback events for delayed work.
  absl::optional<SbEventId> outstanding_delayed_event_;
};

using MessagePumpForUI = MessagePumpUIStarboard;

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_UI_STARBOARD_H_

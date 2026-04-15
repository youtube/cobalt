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
#include "base/synchronization/waitable_event.h"
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
  virtual ~MessagePumpUIStarboard() {
    // 1. Ensure the pump stops processing new tasks.
    Quit();
    // 2. Ensure any pending Starboard event callbacks are cancelled, since the
    //    pump is being destroyed.
    CancelAll();
    // 3. It is critical to call AfterRun() here if a nested RunLoop was still
    //    active when the pump was destroyed. Failing to do so leaves the RunLoop
    //    registered in the SequenceManager's active_run_loops_ stack, which
    //    causes a fatal DCHECK crash during SequenceManager teardown.
    if (run_loop_) {
      run_loop_->AfterRun();
      run_loop_ = nullptr;
    }
  }

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

  // Maintain a RunLoop attached to the starboard thread.
  std::unique_ptr<RunLoop> run_loop_;

  // The MessagePump::Delegate configured in Start().
  Delegate* delegate_;

  bool should_quit() const { return should_quit_; }

  // Flag to support nested RunLoops cleanly. By using
  // `should_quit_`, we can safely use `base::AutoReset` in `Run()` to support
  // re-entrant (nested) invocations of the message pump without corrupting
  // the outer loop's state.
  bool should_quit_ = false;

  // Lock protecting outstanding scheduled callback events.
  base::Lock outstanding_events_lock_;

  // Event to wake up the message pump from a blocking wait during nested
  // loops.
  base::WaitableEvent wakeup_event_{
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED};

  // The set of outstanding scheduled callback events for immediate work.
  absl::optional<SbEventId> outstanding_event_;

  // The set of outstanding scheduled callback events for delayed work.
  absl::optional<SbEventId> outstanding_delayed_event_;
};

using MessagePumpForUI = MessagePumpUIStarboard;

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_UI_STARBOARD_H_

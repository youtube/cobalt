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

#ifndef COBALT_DOM_WINDOW_TIMERS_H_
#define COBALT_DOM_WINDOW_TIMERS_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "cobalt/base/application_state.h"
#include "cobalt/base/debugger_hooks.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class WindowTimers {
 public:
  typedef script::CallbackFunction<void()> TimerCallback;
  typedef script::ScriptValue<TimerCallback> TimerCallbackArg;
  explicit WindowTimers(script::Wrappable* const owner,
                        DomStatTracker* dom_stat_tracker,
                        const base::DebuggerHooks& debugger_hooks,
                        base::ApplicationState application_state)
      : owner_(owner),
        dom_stat_tracker_(dom_stat_tracker),
        debugger_hooks_(debugger_hooks),
        application_state_(application_state) {}
  ~WindowTimers() { DisableCallbacks(); }

  int SetTimeout(const TimerCallbackArg& handler, int timeout);

  void ClearTimeout(int handle);

  int SetInterval(const TimerCallbackArg& handler, int timeout);

  void ClearInterval(int handle);

  // When called, it will irreversibly put the WindowTimers object in an
  // inactive state where timer callbacks are ignored.  This is useful when
  // we're in the process of shutting down and wish to drain the JavaScript
  // event queue without adding more on to the end of it.
  void DisableCallbacks();

  void SetApplicationState(base::ApplicationState state);

 private:
  class Timer : public base::RefCounted<Timer> {
   public:
    enum TimerType { kOneShot, kRepeating };

    Timer(TimerType type, script::Wrappable* const owner,
          DomStatTracker* dom_stat_tracker,
          const base::DebuggerHooks& debugger_hooks,
          const TimerCallbackArg& callback, int timeout, int handle,
          WindowTimers* window_timers);

    base::internal::TimerBase* timer() { return timer_.get(); }
    void Run();

    // Pause this timer. The timer will not fire when paused.
    void Pause();
    // Start or Resume this timer. If the timer was paused and the desired run
    // time is in the past, it will fire immediately.
    void StartOrResume();

   private:
    ~Timer();

    // Create and start a timer of the specified TimerClass type.
    template <class TimerClass>
    std::unique_ptr<base::internal::TimerBase> CreateAndStart();

    TimerType type_;
    std::unique_ptr<base::internal::TimerBase> timer_;
    TimerCallbackArg::Reference callback_;
    DomStatTracker* const dom_stat_tracker_;
    const base::DebuggerHooks& debugger_hooks_;
    int timeout_;
    int handle_;
    WindowTimers* window_timers_;

    // Store the desired run tim of a paused timer.
    base::Optional<base::TimeTicks> desired_run_time_;

    friend class base::RefCounted<Timer>;
  };
  typedef base::hash_map<int, scoped_refptr<Timer> > Timers;

  // Try to add a new timer of the given type, return the handle or 0 when
  // failed.
  int TryAddNewTimer(Timer::TimerType type, const TimerCallbackArg& handler,
                     int timeout);

  // Returns a positive integer timer handle that hasn't been assigned, or 0
  // if none can be found.
  int GetFreeTimerHandle();

  // This callback, when called by Timer, runs the callback in TimerInfo
  // and removes the handle if necessary.
  void RunTimerCallback(int handle);

  Timers timers_;
  int current_timer_index_ = 0;
  script::Wrappable* const owner_;
  DomStatTracker* const dom_stat_tracker_;
  const base::DebuggerHooks& debugger_hooks_;

  // Set to false when we're about to shutdown, to ensure that no new JavaScript
  // is fired as we are waiting for it to drain.
  bool callbacks_active_ = true;

  base::ApplicationState application_state_;

  DISALLOW_COPY_AND_ASSIGN(WindowTimers);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_WINDOW_TIMERS_H_

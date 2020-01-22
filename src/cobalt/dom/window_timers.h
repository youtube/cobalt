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

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/timer/timer.h"
#include "cobalt/base/debugger_hooks.h"
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
                        const base::DebuggerHooks& debugger_hooks)
      : current_timer_index_(0),
        owner_(owner),
        debugger_hooks_(debugger_hooks) {}
  ~WindowTimers() {}

  int SetTimeout(const TimerCallbackArg& handler, int timeout);

  void ClearTimeout(int handle);

  int SetInterval(const TimerCallbackArg& handler, int timeout);

  void ClearInterval(int handle);

  void ClearAllIntervalsAndTimeouts();

  // When called, it will irreversibly put the WindowTimers object in an
  // inactive state where timer callbacks are ignored.  This is useful when
  // we're in the process of shutting down and wish to drain the JavaScript
  // event queue without adding more on to the end of it.
  void DisableCallbacks();

 private:
  class TimerInfo : public base::RefCounted<TimerInfo> {
   public:
    TimerInfo(script::Wrappable* const owner,
              std::unique_ptr<base::internal::TimerBase> timer,
              const TimerCallbackArg& callback)
        : timer_(std::move(timer)), callback_(owner, callback) {}

    base::internal::TimerBase* timer() { return timer_.get(); }
    TimerCallbackArg::Reference& callback_reference() { return callback_; }

   private:
    ~TimerInfo() {}
    std::unique_ptr<base::internal::TimerBase> timer_;
    TimerCallbackArg::Reference callback_;

    friend class base::RefCounted<TimerInfo>;
  };
  typedef base::hash_map<int, scoped_refptr<TimerInfo> > Timers;

  // Returns a positive interger timer handle that hasn't been assigned, or 0
  // if none can be found.
  int GetFreeTimerHandle();

  // This callback, when called by Timer, runs the callback in TimerInfo
  // and removes the handle if necessary.
  void RunTimerCallback(int handle);

  Timers timers_;
  int current_timer_index_;
  script::Wrappable* const owner_;
  const base::DebuggerHooks& debugger_hooks_;

  // Set to false when we're about to shutdown, to ensure that no new JavaScript
  // is fired as we are waiting for it to drain.
  bool callbacks_active_ = true;

  DISALLOW_COPY_AND_ASSIGN(WindowTimers);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_WINDOW_TIMERS_H_

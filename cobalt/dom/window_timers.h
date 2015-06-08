/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef DOM_WINDOW_TIMERS_H_
#define DOM_WINDOW_TIMERS_H_

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "cobalt/script/callback_function.h"

namespace cobalt {
namespace dom {

class WindowTimers {
 public:
  typedef script::CallbackFunction<void()> TimerCallback;
  WindowTimers() : current_timer_index_(0) {}
  ~WindowTimers() {}

  int SetTimeout(const scoped_refptr<TimerCallback>& handler, int timeout);

  void ClearTimeout(int handle);

 private:
  class TimerInfo : public base::RefCounted<TimerInfo> {
   public:
    TimerInfo(scoped_ptr<base::Timer> timer,
              scoped_refptr<TimerCallback> callback)
        : timer_(timer.release()), callback_(callback) {}

    base::Timer* timer() { return timer_.get(); }
    TimerCallback* callback() { return callback_; }

   private:
    ~TimerInfo() {}
    scoped_ptr<base::Timer> timer_;
    scoped_refptr<TimerCallback> callback_;

    friend class base::RefCounted<TimerInfo>;
  };
  typedef base::hash_map<int, scoped_refptr<TimerInfo> > Timers;

  // Returns a positive interger timer handle that hasn't been assigned, or 0
  // if none can be found.
  int GetFreeTimerHandle();

  // This callback, when called by base::Timer, runs the callback in TimerInfo
  // and removes the handle if necessary.
  void RunTimerCallback(int handle);

  Timers timers_;
  int current_timer_index_;

  DISALLOW_COPY_AND_ASSIGN(WindowTimers);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_WINDOW_TIMERS_H_

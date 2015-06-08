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

#include "cobalt/dom/window_timers.h"

#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"

namespace cobalt {
namespace dom {

int WindowTimers::SetTimeout(const scoped_refptr<TimerCallback>& handler,
                             int timeout) {
  int handle = GetFreeTimerHandle();
  DCHECK(handle);
  scoped_ptr<base::Timer> timer(new base::OneShotTimer<TimerInfo>());
  timers_[handle] = new TimerInfo(timer.Pass(), handler);
  static_cast<base::OneShotTimer<TimerInfo>*>(timers_[handle]->timer())
      ->Start(FROM_HERE, base::TimeDelta::FromMilliseconds(timeout),
              base::Bind(&WindowTimers::RunTimerCallback,
                         base::Unretained(this), handle));
  return handle;
}

void WindowTimers::ClearTimeout(int handle) { timers_.erase(handle); }

int WindowTimers::GetFreeTimerHandle() {
  int next_timer_index = current_timer_index_;
  while (true) {
    if (next_timer_index == std::numeric_limits<int>::max()) {
      next_timer_index = 1;
    } else {
      ++next_timer_index;
    }
    if (timers_.find(next_timer_index) == timers_.end()) {
      current_timer_index_ = next_timer_index;
      return current_timer_index_;
    }
    if (next_timer_index == current_timer_index_) {
      break;
    }
  }
  DLOG(INFO) << "No available timer handle.";
  return 0;
}

void WindowTimers::RunTimerCallback(int handle) {
  Timers::iterator timer = timers_.find(handle);
  DCHECK(timer != timers_.end());
  DCHECK(timer->second->callback());
  timer->second->callback()->Run();
  // If the timer is not running, it means it is an oneshot timer and has just
  // fired the shot, and it should be removed now.
  if (!timer->second->timer()->IsRunning()) {
    timers_.erase(timer);
  }
}

}  // namespace dom
}  // namespace cobalt

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

#ifndef COBALT_BASE_POLLER_H_
#define COBALT_BASE_POLLER_H_

#include <memory>

#include "base/callback.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"

namespace base {

// Poller is a wrapper around a base::RepeatingTimer, with simplified semantics.
class Poller {
 public:
  Poller(const Closure& user_task, TimeDelta period) : message_loop_(NULL) {
    StartTimer(user_task, period);
  }

  // Using this constructor, you can specify the message loop you would like
  // the poll task to be run on.
  Poller(base::MessageLoop* message_loop, const Closure& user_task,
         TimeDelta period)
      : message_loop_(message_loop) {
    message_loop_->PostTask(
        FROM_HERE, base::Bind(&Poller::StartTimer, base::Unretained(this),
                              user_task, period));
  }

  ~Poller() {
    // If our timer is running on an explicit message loop, post a task to
    // have it shut down on that message loop before completing the destruction
    // of the Poller.
    if (!message_loop_) {
      StopTimer();
    } else {
      // Wait for the timer to actually be stopped.
      message_loop_->PostBlockingTask(
          FROM_HERE, base::Bind(&Poller::StopTimer, base::Unretained(this)));
    }
  }

 private:
  void StartTimer(const Closure& user_task, TimeDelta period) {
    timer_.reset(new RepeatingTimer<Poller>());
    timer_->Start(FROM_HERE, period, user_task);
  }

  void StopTimer() {
    timer_.reset();
  }

  base::MessageLoop* message_loop_;
  std::unique_ptr<RepeatingTimer<Poller> > timer_;
  DISALLOW_COPY_AND_ASSIGN(Poller);
};

class PollerWithThread {
 public:
  PollerWithThread(const Closure& user_task, TimeDelta period)
      : thread_("PollerThread") {
    // Start the dedicated thread and run the Poller on that thread's
    // message loop.
    thread_.Start();
    poller_.reset(new Poller(thread_.message_loop(), user_task, period));
  }

 private:
  base::Thread thread_;
  std::unique_ptr<Poller> poller_;
};
}  // namespace base

#endif  // COBALT_BASE_POLLER_H_

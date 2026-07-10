// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include <signal.h>
#include <time.h>
#include <unistd.h>

namespace {

// A one-shot POSIX timer used to implement a thread-directed alarm()
struct ThreadAlarmTimer {
  timer_t timer = nullptr;
  bool created = false;

  ~ThreadAlarmTimer() {
    if (created) {
      timer_delete(timer);
    }
  }
};

thread_local ThreadAlarmTimer g_thread_alarm_timer;

}  // namespace

extern "C" {

unsigned int __real_alarm(unsigned int seconds);

// alarm() on Android/ART calls setitimer set with ITIMER_REAL, which raises a
// process-directed SIGALRM. signal() may deliver process-directed signals to
// any of the process threads that has SIGALRM unblocked, and the kernel decides
// which is going to be the receiving thread. So on Android, waiting for SIGALRM
// isn't reliable and to circunvent this issue a per-thread POSIX timer is set
// with SIGEV_THREAD_ID, which sets up the thread that started the timer as the
// receiver, not letting the signal go to unblocked receivers like the ART
// signal catcher.

unsigned int __wrap_alarm(unsigned int seconds) {
  ThreadAlarmTimer& thread_timer = g_thread_alarm_timer;
  if (!thread_timer.created) {
    if (seconds == 0) {
      // No timer to create yet; just cancel any existing process-level alarm.
      return __real_alarm(0);
    }
    struct sigevent sev = {};
    sev.sigev_notify = SIGEV_THREAD_ID;
    sev.sigev_signo = SIGALRM;
    sev.sigev_notify_thread_id = gettid();
    if (timer_create(CLOCK_REALTIME, &sev, &thread_timer.timer) != 0) {
      // fallback
      return __real_alarm(seconds);
    }
    thread_timer.created = true;
  }

  struct itimerspec new_value = {};
  new_value.it_value.tv_sec = seconds;
  struct itimerspec old_value = {};
  if (timer_settime(thread_timer.timer, 0, &new_value, &old_value) != 0) {
    // fallback
    return __real_alarm(seconds);
  }

  // matches musl's implementation return logic, but using
  // nsec instead of usec
  return old_value.it_value.tv_sec + !!old_value.it_value.tv_nsec;
}

}  // extern "C"

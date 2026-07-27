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

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

extern "C" {

int __real_kill(pid_t pid, int sig);

// Redirect a self-directed signal to the calling thread with
// pthread_kill so it cannot be intercepted by another thread's sigwait.
// Prevent the signals from being caught by ART's signal catcher.
int __wrap_kill(pid_t pid, int sig) {
  // sig == 0 is an existence/permission check with no delivery
  // leave it to real kill()
  if (pid == getpid() && sig != 0) {
    int result = pthread_kill(pthread_self(), sig);
    if (result != 0) {
      // pthread_kill returns the error;
      // translate to kill() semantics, setting errno
      errno = result;
      return -1;
    }
    return 0;
  }
  return __real_kill(pid, sig);
}

}  // extern "C"

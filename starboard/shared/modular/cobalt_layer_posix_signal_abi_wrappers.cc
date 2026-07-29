// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <pthread.h>
#include <signal.h>

extern "C" {

// --- Function Declarations for ABI Wrappers ---

int __abi_wrap_sigaction(int signum,
                         const struct sigaction* act,
                         struct sigaction* oldact);

int __abi_wrap_pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset);
int __abi_wrap_pthread_kill(pthread_t thread, int sig);

// --- Standard Function Definitions ---

int sigaction(int signum,
              const struct sigaction* act,
              struct sigaction* oldact) {
  return __abi_wrap_sigaction(signum, act, oldact);
}

int pthread_sigmask(int how, const sigset_t* set, sigset_t* oldset) {
  return __abi_wrap_pthread_sigmask(how, set, oldset);
}

int pthread_kill(pthread_t thread, int sig) {
  return __abi_wrap_pthread_kill(thread, sig);
}

}  // extern "C"

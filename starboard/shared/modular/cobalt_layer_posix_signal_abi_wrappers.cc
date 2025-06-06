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

#include <signal.h>

extern "C" {

// --- Function Declarations for ABI Wrappers ---

int __abi_wrap_sigaction(int signum,
                         const struct sigaction* act,
                         struct sigaction* oldact);

int __abi_wrap_sigprocmask(int how, const sigset_t* set, sigset_t* oldset);

int __abi_wrap_sigemptyset(sigset_t* set);
int __abi_wrap_sigfillset(sigset_t* set);
int __abi_wrap_sigaddset(sigset_t* set, int signum);
int __abi_wrap_sigdelset(sigset_t* set, int signum);
int __abi_wrap_sigismember(const sigset_t* set, int signum);

// --- Standard Function Definitions ---

int sigaction(int signum,
              const struct sigaction* act,
              struct sigaction* oldact) {
  return __abi_wrap_sigaction(signum, act, oldact);
}

int sigprocmask(int how, const sigset_t* set, sigset_t* oldset) {
  return __abi_wrap_sigprocmask(how, set, oldset);
}

int sigemptyset(sigset_t* set) {
  return __abi_wrap_sigemptyset(set);
}

int sigfillset(sigset_t* set) {
  return __abi_wrap_sigfillset(set);
}

int sigaddset(sigset_t* set, int signum) {
  return __abi_wrap_sigaddset(set, signum);
}

int sigdelset(sigset_t* set, int signum) {
  return __abi_wrap_sigdelset(set, signum);
}

int sigismember(const sigset_t* set, int signum) {
  return __abi_wrap_sigismember(set, signum);
}

}  // extern "C"

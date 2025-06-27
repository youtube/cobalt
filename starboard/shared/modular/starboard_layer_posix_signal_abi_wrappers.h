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

#ifndef STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_SIGNAL_ABI_WRAPPERS_H_
#define STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_SIGNAL_ABI_WRAPPERS_H_

#include <signal.h>
#include <stdint.h>
#include <time.h>

#include "starboard/configuration.h"
#include "starboard/export.h"
#include "starboard/shared/modular/starboard_layer_posix_pthread_abi_wrappers.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- MUSL-compatible definitions for signal handling ---

// Values for sa_flags, from musl's bits/signal.h.
#define MUSL_SA_NOCLDSTOP 1
#define MUSL_SA_NOCLDWAIT 2
#define MUSL_SA_SIGINFO 4
#define MUSL_SA_ONSTACK 0x08000000
#define MUSL_SA_RESTART 0x10000000
#define MUSL_SA_NODEFER 0x40000000
#define MUSL_SA_RESETHAND 0x80000000

// Musl's pid_t and uid_t are int and unsigned int respectively.
typedef int musl_pid_t;
typedef unsigned int musl_uid_t;

typedef struct musl_sigset_t {
  unsigned long __bits[128 / sizeof(long)];
} musl_sigset_t;

typedef struct musl_siginfo_t {
  int musl_si_signo, musl_si_errno, musl_si_code;
  union {
    char __pad[128 - 2 * sizeof(int) - sizeof(long)];
    struct {
      union {
        struct {
          musl_pid_t musl_si_pid;
          musl_uid_t musl_si_uid;
        } __piduid;
        struct {
          int musl_si_timerid;
          int musl_si_overrun;
        } __timer;
      } __first;
      union {
        union sigval musl_si_value;
        struct {
          int musl_si_status;
          clock_t musl_si_utime, musl_si_stime;
        } __sigchld;
      } __second;
    } __si_common;
    struct {
      void* musl_si_addr;
      short musl_si_addr_lsb;
      union {
        struct {
          void* musl_si_lower;
          void* musl_si_upper;
        } __addr_bnd;
        unsigned musl_si_pkey;
      } __first;
    } __sigfault;
    struct {
      long musl_si_band;
      int musl_si_fd;
    } __sigpoll;
    struct {
      void* musl_si_call_addr;
      int musl_si_syscall;
      unsigned musl_si_arch;
    } __sigsys;
  } __si_fields;
} musl_siginfo_t;

// Musl-compatible sigaction structure.
// Note the use of musl_sigset_t for the mask.
typedef struct musl_sigaction {
  union {
    void (*musl_sa_handler)(int);
    void (*musl_sa_sigaction)(int, struct musl_siginfo_t*, void*);
  };
  musl_sigset_t sa_mask;
  int sa_flags;
  void (*sa_restorer)(void);
} musl_sigaction;

// --- ABI Wrapper Functions ---

// Wraps the platform's sigaction() call.
// It translates the musl_sigaction struct to the platform's native
// sigaction struct. If SA_SIGINFO is set, it replaces the application's
// sa_sigaction callback with a reverse-wrapper that translates the platform's
// siginfo_t to musl_siginfo_t before calling the original callback.
SB_EXPORT int __abi_wrap_sigaction(int signum,
                                   const struct musl_sigaction* act,
                                   struct musl_sigaction* oldact);

SB_EXPORT int __abi_wrap_pthread_sigmask(int,
                                         const musl_sigset_t* __restrict,
                                         musl_sigset_t* __restrict);
SB_EXPORT int __abi_wrap_pthread_kill(musl_pthread_t thread, int sig);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_SHARED_MODULAR_STARBOARD_LAYER_POSIX_SIGNAL_ABI_WRAPPERS_H_

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

#include "starboard/shared/modular/starboard_layer_posix_signal_abi_wrappers.h"

#include <errno.h>
#include <signal.h>
#include <string.h>

#include "starboard/log.h"
#include "starboard/shared/posix/handle_eintr.h"

namespace {

// Per 'man 7 signal', there are 31 standard signals and 32 real-time signals.
// We define a reasonable upper limit to store sigaction handlers.
constexpr int kMaxSignals = 128;

// Global storage for the original MUSL sa_sigaction function pointers.
// This is necessary for the reverse-wrapper mechanism.
void (*g_musl_sigaction_handlers[kMaxSignals])(int, musl_siginfo_t*, void*);

// --- Type Translation Helpers ---

// Translates MUSL sa_flags to platform-native sa_flags.
int MuslToPlatformFlags(int musl_flags) {
  int platform_flags = 0;
  if (musl_flags & MUSL_SA_NOCLDSTOP) {
    platform_flags |= SA_NOCLDSTOP;
  }
  if (musl_flags & MUSL_SA_NOCLDWAIT) {
    platform_flags |= SA_NOCLDWAIT;
  }
  if (musl_flags & MUSL_SA_SIGINFO) {
    platform_flags |= SA_SIGINFO;
  }
  if (musl_flags & MUSL_SA_ONSTACK) {
    platform_flags |= SA_ONSTACK;
  }
  if (musl_flags & MUSL_SA_RESTART) {
    platform_flags |= SA_RESTART;
  }
  if (musl_flags & MUSL_SA_NODEFER) {
    platform_flags |= SA_NODEFER;
  }
  if (musl_flags & MUSL_SA_RESETHAND) {
    platform_flags |= SA_RESETHAND;
  }
  return platform_flags;
}

// Translates platform-native sa_flags to MUSL sa_flags.
int PlatformToMuslFlags(int platform_flags) {
  int musl_flags = 0;
  if (platform_flags & SA_NOCLDSTOP) {
    musl_flags |= MUSL_SA_NOCLDSTOP;
  }
  if (platform_flags & SA_NOCLDWAIT) {
    musl_flags |= MUSL_SA_NOCLDWAIT;
  }
  if (platform_flags & SA_SIGINFO) {
    musl_flags |= MUSL_SA_SIGINFO;
  }
  if (platform_flags & SA_ONSTACK) {
    musl_flags |= MUSL_SA_ONSTACK;
  }
  if (platform_flags & SA_RESTART) {
    musl_flags |= MUSL_SA_RESTART;
  }
  if (platform_flags & SA_NODEFER) {
    musl_flags |= MUSL_SA_NODEFER;
  }
  if (platform_flags & SA_RESETHAND) {
    musl_flags |= MUSL_SA_RESETHAND;
  }
  return musl_flags;
}

// Translates the platform's siginfo_t to our ABI-stable musl_siginfo_t.
void PlatformToMuslSiginfo(const siginfo_t* platform_si,
                           musl_siginfo_t* musl_si) {
  if (!platform_si || !musl_si) {
    return;
  }

  // It's good practice to zero out the destination struct first to avoid stale
  // data in padding or unmapped fields.
  memset(musl_si, 0, sizeof(musl_siginfo_t));
  // 1. Map the common, top-level fields. These are always valid.
  musl_si->musl_si_signo = platform_si->si_signo;
  musl_si->musl_si_errno = platform_si->si_errno;
  musl_si->musl_si_code = platform_si->si_code;

  // 2. Map the union fields based on the signal number and code.
  //    The content of the union depends on what kind of signal this is.

  switch (platform_si->si_signo) {
    case SIGCHLD:
      // Information about a terminated or stopped child process.
      musl_si->__si_fields.__si_common.__first.__piduid.musl_si_pid =
          platform_si->si_pid;
      musl_si->__si_fields.__si_common.__first.__piduid.musl_si_uid =
          platform_si->si_uid;
      musl_si->__si_fields.__si_common.__second.__sigchld.musl_si_status =
          platform_si->si_status;
      musl_si->__si_fields.__si_common.__second.__sigchld.musl_si_utime =
          platform_si->si_utime;
      musl_si->__si_fields.__si_common.__second.__sigchld.musl_si_stime =
          platform_si->si_stime;
      break;

    case SIGSEGV:
    case SIGBUS:
    case SIGILL:
    case SIGFPE:
      // Fault signals provide the memory address that caused the fault.
      musl_si->__si_fields.__sigfault.musl_si_addr = platform_si->si_addr;
      musl_si->__si_fields.__sigfault.musl_si_addr_lsb =
          platform_si->si_addr_lsb;

      // The inner union depends on the specific fault code.
      // For example, SEGV_BNDERR on x86 provides bounds information.
      if (platform_si->si_code == SEGV_BNDERR ||
          platform_si->si_code == BUS_ADRERR) {
        musl_si->__si_fields.__sigfault.__first.__addr_bnd.musl_si_lower =
            platform_si->si_lower;
        musl_si->__si_fields.__sigfault.__first.__addr_bnd.musl_si_upper =
            platform_si->si_upper;
      } else if (platform_si->si_code == SEGV_PKUERR) {
        musl_si->__si_fields.__sigfault.__first.musl_si_pkey =
            platform_si->si_pkey;
      }
      break;

    case SIGPOLL:  // Also known as SIGIO
      // Asynchronous I/O event.
      musl_si->__si_fields.__sigpoll.musl_si_band = platform_si->si_band;
      musl_si->__si_fields.__sigpoll.musl_si_fd = platform_si->si_fd;
      break;

    case SIGSYS:
      // Bad system call.
      musl_si->__si_fields.__sigsys.musl_si_call_addr =
          platform_si->si_call_addr;
      musl_si->__si_fields.__sigsys.musl_si_syscall = platform_si->si_syscall;
      musl_si->__si_fields.__sigsys.musl_si_arch = platform_si->si_arch;
      break;

    default:
      // This section handles other signals, primarily those generated by
      // POSIX timers or sent explicitly with sigqueue().

      // Real-time signals from sigqueue() or other user-space sources.
      // SI_USER means sent by kill(), SI_QUEUE means sent by sigqueue().
      if (platform_si->si_code <= 0) {  // e.g., SI_USER, SI_QUEUE, SI_TKILL
        musl_si->__si_fields.__si_common.__first.__piduid.musl_si_pid =
            platform_si->si_pid;
        musl_si->__si_fields.__si_common.__first.__piduid.musl_si_uid =
            platform_si->si_uid;
        musl_si->__si_fields.__si_common.__second.musl_si_value =
            platform_si->si_value;
      } else {
        // Kernel-generated signals that use the common structure, like POSIX
        // timers. The check for SI_TIMER is a common case.
        if (platform_si->si_code == SI_TIMER) {
          musl_si->__si_fields.__si_common.__first.__timer.musl_si_timerid =
              platform_si->si_timerid;
          musl_si->__si_fields.__si_common.__first.__timer.musl_si_overrun =
              platform_si->si_overrun;
          musl_si->__si_fields.__si_common.__second.musl_si_value =
              platform_si->si_value;
        }
      }
      break;
  }
}

// Reverse-wrapper to be registered with the kernel.
// It translates the platform siginfo_t and calls the stored MUSL handler.
void ReverseWrapperHandler(int signum, siginfo_t* info, void* context) {
  if (signum < 0 || signum >= kMaxSignals) {
    // This should not happen if checked in sigaction wrapper.
    return;
  }

  void (*musl_handler)(int, musl_siginfo_t*, void*) =
      g_musl_sigaction_handlers[signum];
  if (musl_handler) {
    musl_siginfo_t musl_si;
    PlatformToMuslSiginfo(info, &musl_si);
    musl_handler(signum, &musl_si, nullptr);
  }
}

}  // namespace

// --- ABI Wrapper Implementations ---

extern "C" {

SB_EXPORT int __abi_wrap_sigaction(int signum,
                                   const struct musl_sigaction* act,
                                   struct musl_sigaction* oldact) {
  if (signum < 0 || signum >= kMaxSignals) {
    errno = EINVAL;
    return -1;
  }

  struct sigaction platform_act;
  struct sigaction* platform_act_ptr = nullptr;
  if (act) {
    platform_act_ptr = &platform_act;
    memset(&platform_act, 0, sizeof(platform_act));

    platform_act.sa_flags = MuslToPlatformFlags(act->sa_flags);
    // The musl_sigset_t has a compatible layout with sigset_t.
    memcpy(&platform_act.sa_mask, &act->sa_mask, sizeof(act->sa_mask));

    if (act->sa_flags & MUSL_SA_SIGINFO) {
      // Store the musl handler and register our reverse-wrapper.
      g_musl_sigaction_handlers[signum] = act->musl_sa_sigaction;
      platform_act.sa_sigaction = ReverseWrapperHandler;
    } else {
      platform_act.sa_handler = act->musl_sa_handler;
    }
  }

  struct sigaction platform_oldact;
  struct sigaction* platform_oldact_ptr = nullptr;
  if (oldact) {
    platform_oldact_ptr = &platform_oldact;
  }

  int result =
      HANDLE_EINTR(sigaction(signum, platform_act_ptr, platform_oldact_ptr));

  if (result == 0 && oldact) {
    memset(oldact, 0, sizeof(*oldact));
    oldact->sa_flags = PlatformToMuslFlags(platform_oldact.sa_flags);
    memcpy(&oldact->sa_mask, &platform_oldact.sa_mask, sizeof(oldact->sa_mask));

    if (oldact->sa_flags & MUSL_SA_SIGINFO) {
      // The actual handler is our wrapper, but we return the original one.
      oldact->musl_sa_sigaction = g_musl_sigaction_handlers[signum];
    } else {
      oldact->musl_sa_handler = platform_oldact.sa_handler;
    }
  }

  return result;
}

SB_EXPORT int __abi_wrap_sigprocmask(int how,
                                     const musl_sigset_t* set,
                                     musl_sigset_t* oldset) {
  return sigprocmask(how, reinterpret_cast<const sigset_t*>(set),
                     reinterpret_cast<sigset_t*>(oldset));
}

SB_EXPORT int __abi_wrap_sigemptyset(musl_sigset_t* set) {
  return sigemptyset(reinterpret_cast<sigset_t*>(set));
}

SB_EXPORT int __abi_wrap_sigfillset(musl_sigset_t* set) {
  return sigfillset(reinterpret_cast<sigset_t*>(set));
}

SB_EXPORT int __abi_wrap_sigaddset(musl_sigset_t* set, int signum) {
  return sigaddset(reinterpret_cast<sigset_t*>(set), signum);
}

SB_EXPORT int __abi_wrap_sigdelset(musl_sigset_t* set, int signum) {
  return sigdelset(reinterpret_cast<sigset_t*>(set), signum);
}

SB_EXPORT int __abi_wrap_sigismember(const musl_sigset_t* set, int signum) {
  return sigismember(reinterpret_cast<const sigset_t*>(set), signum);
}

}  // extern "C"

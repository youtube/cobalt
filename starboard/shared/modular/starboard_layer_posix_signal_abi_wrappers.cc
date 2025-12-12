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
#include "starboard/shared/modular/starboard_layer_posix_errno_abi_wrappers.h"
#include "starboard/shared/posix/handle_eintr.h"

namespace {

// Per 'man 7 signal', there are 31 standard signals and 32 real-time signals.
// We define a reasonable upper limit to store sigaction handlers.
constexpr int kMaxSignals = 128;

// Global storage for the original MUSL sa_sigaction function pointers.
// This is necessary for the reverse-wrapper mechanism.
void (*g_musl_sigaction_handlers[kMaxSignals])(int,
                                               musl_siginfo_t*,
                                               void*) = {};

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
  *musl_si = {};
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
          static_cast<musl_pid_t>(platform_si->si_pid);
      musl_si->__si_fields.__si_common.__first.__piduid.musl_si_uid =
          static_cast<musl_uid_t>(platform_si->si_uid);
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
            static_cast<musl_pid_t>(platform_si->si_pid);
        musl_si->__si_fields.__si_common.__first.__piduid.musl_si_uid =
            static_cast<musl_uid_t>(platform_si->si_uid);
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
  if (!musl_handler) {
    return;
  }
  musl_siginfo_t musl_si;
  PlatformToMuslSiginfo(info, &musl_si);
  // The third argument to the sa_sigaction handler is a ucontext_t*,
  // which is a machine-dependent and opaque structure per 'man getcontext'.
  // Since Cobalt does not currently use getcontext/setcontext, and any
  // code that dereferences this structure would be non-portable, we pass
  // nullptr.
  musl_handler(signum, &musl_si, nullptr);
}

void musl_sigemptyset(musl_sigset_t* set) {
  if (set == nullptr) {
    errno = EINVAL;
    return;
  }
  for (size_t i = 0; i < sizeof(set->__bits) / sizeof(set->__bits[0]); ++i) {
    set->__bits[i] = 0UL;
  }
}

// Define a maximum signal number for validation, assuming signals 1-127 are
// possible based on the 128-bit capacity of the sigset_t (128 bits total / 8
// bits per byte = 16 bytes for long on 64-bit system, so 128 / (sizeof(long)*8)
// = 128 / 64 = 2 elements in __bits for a 64-bit long system). If sizeof(long)
// is 4 bytes (32-bit system), then 128 / (4*8) = 128 / 32 = 4 elements. The
// total bits are 128, so max signal can effectively be 127 (0-indexed).
constexpr int kMaxSignalNumber = 127;

int musl_sigaddset(musl_sigset_t* set, int signum) {
  if (set == nullptr || signum <= 0 || signum > kMaxSignalNumber) {
    errno = EINVAL;
    return -1;
  }
  // Convert 1-indexed signum to 0-indexed bit position 's'.
  // POSIX signals are >= 1.
  unsigned s = signum - 1;

  size_t bit_index = s / (sizeof(unsigned long) * 8);
  size_t bit_pos = s % (sizeof(unsigned long) * 8);
  set->__bits[bit_index] |= (1UL << bit_pos);

  return 0;
}

int musl_sigismember(const musl_sigset_t* set, int signum) {
  if (set == nullptr || signum <= 0 || signum > kMaxSignalNumber) {
    errno = EINVAL;
    return -1;
  }

  // Convert 1-indexed signum to 0-indexed bit position 's'.
  // POSIX signals are >= 1.
  unsigned s = signum - 1;

  size_t bit_index = s / (sizeof(unsigned long) * 8);
  size_t bit_pos = s % (sizeof(unsigned long) * 8);

  return ((set->__bits[bit_index] & (1UL << bit_pos)) != 0);
}

}  // namespace

// --- ABI Wrapper Implementations ---

int __abi_wrap_sigaction(int signum,
                         const struct musl_sigaction* act,
                         struct musl_sigaction* oldact) {
  if (signum < 0 || signum >= kMaxSignals) {
    errno = EINVAL;
    return -1;
  }

  struct sigaction platform_act{};
  struct sigaction* platform_act_ptr = nullptr;
  if (act) {
    platform_act_ptr = &platform_act;

    platform_act.sa_flags = MuslToPlatformFlags(act->sa_flags);

    if (sigemptyset(&platform_act.sa_mask) == -1) {
      return -1;
    }
    for (int i = 1; i < NSIG; ++i) {
      if (i <= kMaxSignalNumber && musl_sigismember(&act->sa_mask, i)) {
        if (sigaddset(&platform_act.sa_mask, i) == -1) {
          return -1;
        }
      }
    }

    if (act->sa_flags & MUSL_SA_SIGINFO) {
      // Store the musl handler and register our reverse-wrapper.
      g_musl_sigaction_handlers[signum] = act->musl_sa_sigaction;
      platform_act.sa_sigaction = ReverseWrapperHandler;
    } else {
      // Not a SA_SIGINFO handler, so clear any existing reverse-wrapper
      // handler.
      g_musl_sigaction_handlers[signum] = nullptr;
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
    *oldact = {};
    oldact->sa_flags = PlatformToMuslFlags(platform_oldact.sa_flags);

    // Translate the platform's sigset_t back to the musl_sigset_t.
    musl_sigemptyset(&oldact->sa_mask);
    for (int i = 1; i < NSIG; ++i) {
      if (i <= kMaxSignalNumber && sigismember(&platform_oldact.sa_mask, i)) {
        musl_sigaddset(&oldact->sa_mask, i);
      }
    }

    if (oldact->sa_flags & MUSL_SA_SIGINFO) {
      // The actual handler is our wrapper, but we return the original one.
      oldact->musl_sa_sigaction = g_musl_sigaction_handlers[signum];
    } else {
      oldact->musl_sa_handler = platform_oldact.sa_handler;
    }
  }

  return result;
}

int __abi_wrap_pthread_sigmask(int how,
                               const musl_sigset_t* set,
                               musl_sigset_t* oldset) {
  sigset_t native_set;
  sigset_t native_oldset;
  sigset_t* p_native_set = nullptr;
  sigset_t* p_native_oldset = nullptr;

  // Handle the 'set' parameter: Translate musl_sigset_t to native sigset_t
  if (set) {
    // Initialize the native sigset to empty first
    if (sigemptyset(&native_set) == -1) {
      return -1;
    }

    // Iterate through all possible signal numbers and add them if present in
    // musl_set
    for (int i = 1; i < NSIG; ++i) {
      if (i <= kMaxSignalNumber && musl_sigismember(set, i) == 1) {
        if (sigaddset(&native_set, i) == -1) {
          return -1;
        }
      }
    }
    p_native_set = &native_set;
  }

  if (oldset) {
    p_native_oldset = &native_oldset;
  }

  int ret = pthread_sigmask(how, p_native_set, p_native_oldset);

  // If pthread_sigmask was successful and oldset was requested, translate
  // native_oldset back to musl_sigset_t
  if (ret == 0 && oldset) {
    musl_sigemptyset(oldset);
    for (int i = 1; i < NSIG; ++i) {
      if (i <= kMaxSignalNumber && sigismember(&native_oldset, i) == 1) {
        musl_sigaddset(oldset, i);
      }
    }
  }
  return ret;
}

int __abi_wrap_pthread_kill(musl_pthread_t thread, int sig) {
  int ret = pthread_kill(reinterpret_cast<pthread_t>(thread), sig);
  return errno_to_musl_errno(ret);
}

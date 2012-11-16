// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/stack_trace.h"

#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"

namespace base {
namespace debug {

bool EnableInProcessStackDumping() {
  // When running in an application, our code typically expects SIGPIPE
  // to be ignored.  Therefore, when testing that same code, it should run
  // with SIGPIPE ignored as well.
  // TODO(phajdan.jr): De-duplicate this SIGPIPE code.
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_IGN;
  sigemptyset(&action.sa_mask);
  return (sigaction(SIGPIPE, &action, NULL) == 0);
}

StackTrace::StackTrace() {
}

// Sends fake SIGSTKFLT signals to let the Android linker and debuggerd dump
// stack. See inlined comments and Android bionic/linker/debugger.c and
// system/core/debuggerd/debuggerd.c for details.
void StackTrace::PrintBacktrace() const {
  // Get the current handler of SIGSTKFLT for later use.
  sighandler_t sig_handler = signal(SIGSTKFLT, SIG_DFL);
  signal(SIGSTKFLT, sig_handler);

  // The Android linker will handle this signal and send a stack dumping request
  // to debuggerd which will ptrace_attach this process. Before returning from
  // the signal handler, the linker sets the signal handler to SIG_IGN.
  kill(gettid(), SIGSTKFLT);

  // Because debuggerd will wait for the process to be stopped by the actual
  // signal in crashing scenarios, signal is sent again to met the expectation.
  // Debuggerd will dump stack into the system log and /data/tombstones/ files.
  // NOTE: If this process runs in the interactive shell, it will be put
  // in the background. To resume it in the foreground, use 'fg' command.
  kill(gettid(), SIGSTKFLT);

  // Restore the signal handler so that this method can work the next time.
  signal(SIGSTKFLT, sig_handler);
}

void StackTrace::OutputToStream(std::ostream* os) const {
  NOTIMPLEMENTED();
}

}  // namespace debug
}  // namespace base

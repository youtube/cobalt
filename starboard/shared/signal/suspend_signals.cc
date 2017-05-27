// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "starboard/shared/signal/suspend_signals.h"

#include <signal.h>
#include <sys/socket.h>

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/memory.h"
#include "starboard/shared/signal/signal_internal.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/system.h"

namespace starboard {
namespace shared {
namespace signal {

namespace {

int UnblockSignal(int signal_id) {
  sigset_t mask;
  ::sigemptyset(&mask);
  ::sigaddset(&mask, signal_id);

  sigset_t previous_mask;
  return ::sigprocmask(SIG_UNBLOCK, &mask, &previous_mask);
}

void SetSignalHandler(int signal_id, SignalHandlerFunction handler) {
  struct sigaction action = {0};

  action.sa_handler = handler;
  action.sa_flags = 0;
  ::sigemptyset(&action.sa_mask);

  ::sigaction(signal_id, &action, NULL);
}

void SuspendDone(void* /*context*/) {
  // Stop all thread execution after fully transitioning into Suspended.
  raise(SIGSTOP);
}

void Suspend(int signal_id) {
  LogSignalCaught(signal_id);
  starboard::Application::Get()->Suspend(NULL, &SuspendDone);
}

void Resume(int signal_id) {
  LogSignalCaught(signal_id);
  // TODO: Resume or Unpause based on state before suspend?
  starboard::Application::Get()->Unpause(NULL, NULL);
}

void Ignore(int signal_id) {
  LogSignalCaught(signal_id);
  SbLogRawDumpStack(1);
  SbLogFlush();
}

}  // namespace

#if !defined(MSG_NOSIGNAL) && defined(SO_NOSIGPIPE)
// See "#if !defined(MSG_NOSIGNAL)" below.
// OS X, which we do not build for today, has another mechanism which
// should be used.
#error On this platform, please use SO_NOSIGPIPE and leave the SIGPIPE \
       handler at default.
#endif

void InstallSuspendSignalHandlers() {
#if !defined(MSG_NOSIGNAL)
  // By default in POSIX, sending to a closed socket causes a SIGPIPE
  // If we cannot disable that behavior, we must ignore SIGPIPE.
  // Ignoring SIGPIPE means cases that use pipes to redirect the stdio
  // log messages may behave in surprising ways, so it's not desirable.
  SetSignalHandler(SIGPIPE, &Ignore);
#endif
  SetSignalHandler(SIGUSR1, &Suspend);
  UnblockSignal(SIGUSR1);
  SetSignalHandler(SIGCONT, &Resume);
}

void UninstallSuspendSignalHandlers() {
#if !defined(MSG_NOSIGNAL)
  SetSignalHandler(SIGPIPE, SIG_DFL);
#endif
  SetSignalHandler(SIGUSR1, SIG_DFL);
  SetSignalHandler(SIGCONT, SIG_DFL);
}

}  // namespace signal
}  // namespace shared
}  // namespace starboard

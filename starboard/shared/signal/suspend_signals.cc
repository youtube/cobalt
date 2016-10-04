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

}  // namespace

void InstallSuspendSignalHandlers() {
  SetSignalHandler(SIGTSTP, &Suspend);
  UnblockSignal(SIGTSTP);
  SetSignalHandler(SIGCONT, &Resume);
}

void UninstallSuspendSignalHandlers() {
  SetSignalHandler(SIGCONT, SIG_DFL);
  SetSignalHandler(SIGTSTP, SIG_DFL);
}

}  // namespace signal
}  // namespace shared
}  // namespace starboard

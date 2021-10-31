// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/log.h"
#include "starboard/common/thread.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "starboard/shared/signal/signal_internal.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/system.h"

#if SB_IS(EVERGREEN_COMPATIBLE) && !SB_IS(EVERGREEN_COMPATIBLE_LITE)
#include "starboard/loader_app/pending_restart.h"
#endif  // SB_IS(EVERGREEN_COMPATIBLE) && !SB_IS(EVERGREEN_COMPATIBLE_LITE)

namespace starboard {
namespace shared {
namespace signal {

namespace {

const std::initializer_list<int> kAllSignals = {SIGUSR1, SIGUSR2, SIGCONT,
                                                SIGTSTP, SIGPWR};

int SignalMask(std::initializer_list<int> signal_ids, int action) {
  sigset_t mask;
  ::sigemptyset(&mask);
  for (auto signal_id : signal_ids) {
    ::sigaddset(&mask, signal_id);
  }

  sigset_t previous_mask;
  return ::sigprocmask(action, &mask, &previous_mask);
}

void SetSignalHandler(int signal_id, SignalHandlerFunction handler) {
  struct sigaction action = {0};

  action.sa_handler = handler;
  action.sa_flags = 0;
  ::sigemptyset(&action.sa_mask);

  ::sigaction(signal_id, &action, NULL);
}

#if SB_API_VERSION >= 13
void Conceal(int signal_id) {
  SignalMask(kAllSignals, SIG_BLOCK);
  LogSignalCaught(signal_id);
  SbSystemRequestConceal();
  SignalMask(kAllSignals, SIG_UNBLOCK);
}

void Focus(int signal_id) {
  SignalMask(kAllSignals, SIG_BLOCK);
  LogSignalCaught(signal_id);
  // TODO: Unfreeze or Focus based on state before frozen?
  starboard::Application::Get()->Focus(NULL, NULL);
  SignalMask(kAllSignals, SIG_UNBLOCK);
}

void Freeze(int signal_id) {
  SignalMask(kAllSignals, SIG_BLOCK);
  LogSignalCaught(signal_id);
  SbSystemRequestFreeze();
  SignalMask(kAllSignals, SIG_UNBLOCK);
}

void Stop(int signal_id) {
  SignalMask(kAllSignals, SIG_BLOCK);
  LogSignalCaught(signal_id);
  starboard::Application::Get()->Stop(0);
  SignalMask(kAllSignals, SIG_UNBLOCK);
}
#else
void Suspend(int signal_id) {
  SignalMask(kAllSignals, SIG_BLOCK);
  LogSignalCaught(signal_id);
  SbSystemRequestSuspend();
  SignalMask(kAllSignals, SIG_UNBLOCK);
}

void Resume(int signal_id) {
  SignalMask(kAllSignals, SIG_BLOCK);
  LogSignalCaught(signal_id);
  // TODO: Resume or Unpause based on state before suspend?
  starboard::Application::Get()->Unpause(NULL, NULL);
  SignalMask(kAllSignals, SIG_UNBLOCK);
}
#endif  // SB_API_VERSION >= 13

void LowMemory(int signal_id) {
  SignalMask(kAllSignals, SIG_BLOCK);
  LogSignalCaught(signal_id);
  starboard::Application::Get()->InjectLowMemoryEvent();
  SignalMask(kAllSignals, SIG_UNBLOCK);
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

class SignalHandlerThread : public ::starboard::Thread {
 public:
  SignalHandlerThread() : Thread("SignalHandlTh") {}

  void Run() override {
    SignalMask(kAllSignals, SIG_UNBLOCK);
    while (!WaitForJoin(kSbTimeMax)) {
    }
  }
};

void ConfigureSignalHandlerThread(bool start) {
  static SignalHandlerThread handlerThread;
  if (start) {
    handlerThread.Start();
  } else {
    handlerThread.Join();
  }
}

void InstallSuspendSignalHandlers() {
#if !defined(MSG_NOSIGNAL)
  // By default in POSIX, sending to a closed socket causes a SIGPIPE
  // If we cannot disable that behavior, we must ignore SIGPIPE.
  // Ignoring SIGPIPE means cases that use pipes to redirect the stdio
  // log messages may behave in surprising ways, so it's not desirable.
  SetSignalHandler(SIGPIPE, &Ignore);
#endif
  // Signal handlers are guaranteed to run on dedicated thread by
  // blocking them first on the main thread calling this function early.
  // Future created threads inherit the same block mask as per POSIX rules
  // http://pubs.opengroup.org/onlinepubs/009695399/functions/xsh_chap02_04.html
  SignalMask(kAllSignals, SIG_BLOCK);

#if SB_API_VERSION >= 13
  SetSignalHandler(SIGUSR1, &Conceal);
  SetSignalHandler(SIGUSR2, &LowMemory);
  SetSignalHandler(SIGCONT, &Focus);
  SetSignalHandler(SIGTSTP, &Freeze);
  SetSignalHandler(SIGPWR, &Stop);
  ConfigureSignalHandlerThread(true);
#else
  SetSignalHandler(SIGUSR1, &Suspend);
  SetSignalHandler(SIGUSR2, &LowMemory);
  SetSignalHandler(SIGCONT, &Resume);
  ConfigureSignalHandlerThread(true);
#endif  // SB_API_VERSION >= 13
}

void UninstallSuspendSignalHandlers() {
#if !defined(MSG_NOSIGNAL)
  SetSignalHandler(SIGPIPE, SIG_DFL);
#endif
  SetSignalHandler(SIGUSR1, SIG_DFL);
  SetSignalHandler(SIGUSR2, SIG_DFL);
  SetSignalHandler(SIGCONT, SIG_DFL);
#if SB_API_VERSION >= 13
  SetSignalHandler(SIGPWR, SIG_DFL);
#endif  // SB_API_VERSION >= 13
  ConfigureSignalHandlerThread(false);
}

}  // namespace signal
}  // namespace shared
}  // namespace starboard

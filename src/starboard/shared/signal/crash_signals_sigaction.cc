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

#include "starboard/shared/signal/crash_signals.h"

#include <signal.h>

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/shared/signal/signal_internal.h"
#include "starboard/system.h"

namespace starboard {
namespace shared {
namespace signal {

namespace {

const int kCrashSignalsToTrap[] = {
    SIGABRT, SIGFPE, SIGILL, SIGSEGV,
};

const int kStopSignalsToTrap[] = {
    SIGTERM, SIGINT,
};

void SetSignalHandler(int signal_id, SignalHandlerFunction handler) {
  struct sigaction action = {0};

  action.sa_handler = handler;
  action.sa_flags = 0;
  ::sigemptyset(&action.sa_mask);

  ::sigaction(signal_id, &action, NULL);
}

void DumpStackSignalSafe(int signal_id) {
  LogSignalCaught(signal_id);
  SbLogFlush();
  SbLogRawDumpStack(1);

  UninstallCrashSignalHandlers();
  SbSystemBreakIntoDebugger();
}

void Stop(int signal_id) {
  LogSignalCaught(signal_id);
  SbSystemRequestStop(0);
}

}  // namespace

void InstallCrashSignalHandlers() {
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kCrashSignalsToTrap); ++i) {
    SetSignalHandler(kCrashSignalsToTrap[i], &DumpStackSignalSafe);
  }
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kStopSignalsToTrap); ++i) {
    SetSignalHandler(kStopSignalsToTrap[i], &Stop);
  }
}

void UninstallCrashSignalHandlers() {
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kCrashSignalsToTrap); ++i) {
    SetSignalHandler(kCrashSignalsToTrap[i], SIG_DFL);
  }
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kStopSignalsToTrap); ++i) {
    SetSignalHandler(kStopSignalsToTrap[i], SIG_DFL);
  }
}

}  // namespace signal
}  // namespace shared
}  // namespace starboard

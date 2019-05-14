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

#include "starboard/shared/signal/crash_signals.h"

#include <signal.h>

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/shared/signal/signal_internal.h"
#include "starboard/system.h"

namespace starboard {
namespace shared {
namespace signal {

namespace {

const int kCrashSignalsToTrap[] = {
    SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV, SIGSYS,
};

const int kStopSignalsToTrap[] = {
    SIGHUP,
};

void Crash(int signal_id) {
  DumpStackSignalSafe(signal_id);
  UninstallCrashSignalHandlers();
  SbSystemBreakIntoDebugger();
}

void Quit(int signal_id) {
  DumpStackSignalSafe(signal_id);
  ::signal(SIGQUIT, SIG_DFL);
  ::raise(SIGQUIT);
}

void Stop(int signal_id) {
  LogSignalCaught(signal_id);
  SbSystemRequestStop(0);
}

}  // namespace

void InstallCrashSignalHandlers() {
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kCrashSignalsToTrap); ++i) {
    ::signal(kCrashSignalsToTrap[i], &Crash);
  }
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kStopSignalsToTrap); ++i) {
    ::signal(kStopSignalsToTrap[i], &Stop);
  }
  ::signal(SIGQUIT, &Quit);
}

void UninstallCrashSignalHandlers() {
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kCrashSignalsToTrap); ++i) {
    ::signal(kCrashSignalsToTrap[i], SIG_DFL);
  }
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kStopSignalsToTrap); ++i) {
    ::signal(kStopSignalsToTrap[i], SIG_DFL);
  }
  ::signal(SIGQUIT, SIG_DFL);
}

}  // namespace signal
}  // namespace shared
}  // namespace starboard

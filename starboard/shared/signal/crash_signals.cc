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
#include "starboard/system.h"

namespace starboard {
namespace shared {
namespace signal {

namespace {

const int kSignalsToTrap[] = {
    SIGABRT, SIGFPE, SIGILL, SIGINT, SIGSEGV,
};

const char* GetSignalName(int signal_id) {
  switch (signal_id) {
    case SIGABRT:
      return "SIGABRT";
    case SIGFPE:
      return "SIGFPE";
    case SIGILL:
      return "SIGILL";
    case SIGINT:
      return "SIGINT";
    case SIGSEGV:
      return "SIGSEGV";
    default:
      return "UNKNOWN SIGNAL";
  }
}

void DumpStackSignalSafe(int signal_id) {
  const char* signal_name = GetSignalName(signal_id);
  SbLogRawFormatF("\nCaught signal: %s (%d)\n", signal_name, signal_id);
  SbLogFlush();
  SbLogRawDumpStack(1);

  UninstallCrashSignalHandlers();
  SbSystemBreakIntoDebugger();
}

}  // namespace

void InstallCrashSignalHandlers() {
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kSignalsToTrap); ++i) {
    ::signal(kSignalsToTrap[i], &DumpStackSignalSafe);
  }
}

void UninstallCrashSignalHandlers() {
  for (int i = 0; i < SB_ARRAY_SIZE_INT(kSignalsToTrap); ++i) {
    ::signal(kSignalsToTrap[i], SIG_DFL);
  }
}

}  // namespace signal
}  // namespace shared
}  // namespace starboard

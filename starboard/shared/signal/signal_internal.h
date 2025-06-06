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

#ifndef STARBOARD_SHARED_SIGNAL_SIGNAL_INTERNAL_H_
#define STARBOARD_SHARED_SIGNAL_SIGNAL_INTERNAL_H_

#include <signal.h>

#include "starboard/common/log.h"
#include "starboard/shared/internal_only.h"

namespace starboard::shared::signal {

inline const char* GetSignalName(int signal_id) {
  switch (signal_id) {
    case SIGABRT:
      return "SIGABRT";
    case SIGBUS:
      return "SIGBUS";
    case SIGCONT:
      return "SIGCONT";
    case SIGFPE:
      return "SIGFPE";
    case SIGHUP:
      return "SIGHUP";
    case SIGILL:
      return "SIGILL";
    case SIGINT:
      return "SIGINT";
    case SIGPIPE:
      return "SIGPIPE";
    case SIGPWR:
      return "SIGPWR";
    case SIGQUIT:
      return "SIGQUIT";
    case SIGSEGV:
      return "SIGSEGV";
    case SIGSYS:
      return "SIGSYS";
    case SIGTSTP:
      return "SIGTSTP";
    case SIGTERM:
      return "SIGTERM";
    case SIGUSR1:
      return "SIGUSR1";
    case SIGUSR2:
      return "SIGUSR2";
    default:
      return "UNKNOWN";
  }
}

inline void LogSignalCaught(int signal_id) {
  const char* signal_name = GetSignalName(signal_id);
  SbLogRawFormatF("\nCaught signal: %s (%d)\n", signal_name, signal_id);
  SbLogFlush();
}

inline void DumpStackSignalSafe(int signal_id) {
  LogSignalCaught(signal_id);
  SbLogRawDumpStack(2);
  SbLogFlush();
}

typedef void (*SignalHandlerFunction)(int);

}  // namespace starboard::shared::signal

#endif  // STARBOARD_SHARED_SIGNAL_SIGNAL_INTERNAL_H_

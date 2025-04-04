// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/multiprocess_test.h"

#include "base/command_line.h"
#include "base/threading/thread_restrictions.h"

namespace base {

Process SpawnMultiProcessTestChild(const std::string&,
                                   const CommandLine&,
                                   const LaunchOptions&) {
  return Process();
}

bool WaitForMultiprocessTestChildExit(const Process&,
                                      TimeDelta,
                                      int*) {
  return false;
}

bool TerminateMultiProcessTestChild(const Process& process,
                                    int exit_code,
                                    bool wait) {
  return false;
}

CommandLine GetMultiProcessTestChildBaseCommandLine() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CommandLine cmd_line = *CommandLine::ForCurrentProcess();
  return cmd_line;
}

// MultiProcessTest ------------------------------------------------------------

MultiProcessTest::MultiProcessTest() = default;

Process MultiProcessTest::SpawnChild(const std::string&) {
  return Process();
}

Process MultiProcessTest::SpawnChildWithOptions(const std::string&,
                                                const LaunchOptions&) {
  return Process();
}

CommandLine MultiProcessTest::MakeCmdLine(const std::string&) {
  CommandLine command_line = GetMultiProcessTestChildBaseCommandLine();
  return command_line;
}

}  // namespace base

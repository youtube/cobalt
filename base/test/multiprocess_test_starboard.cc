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

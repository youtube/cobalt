//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
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

#include <signal.h>
#include <sys/resource.h>

#include <cstring>

#include "starboard/configuration.h"
#include "starboard/shared/signal/crash_signals.h"
#include "starboard/shared/signal/suspend_signals.h"

#include "third_party/starboard/rdk/shared/application_rdk.h"

#if SB_IS(EVERGREEN_COMPATIBLE)
#include "starboard/common/command_line.h"
#include "starboard/common/paths.h"
#include "starboard/crashpad_wrapper/wrapper.h"
#include "starboard/elf_loader/elf_loader_constants.h"
#endif

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

static struct sigaction old_actions[2];

static void RequestStop(int signal_id) {
  SbSystemRequestStop(0);
}

static void InstallStopSignalHandlers() {
  struct sigaction action;
  memset (&action, 0, sizeof (action));
  action.sa_handler = RequestStop;
  action.sa_flags = 0;
  ::sigemptyset(&action.sa_mask);
  ::sigaction(SIGINT, &action, &old_actions[0]);
  ::sigaction(SIGTERM, &action, &old_actions[1]);
}

static void UninstallStopSignalHandlers() {
  ::sigaction(SIGINT, &old_actions[0], NULL);
  ::sigaction(SIGTERM, &old_actions[1], NULL);
}

}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party

extern "C" SB_EXPORT_PLATFORM int StarboardMain(int argc, char** argv) {
  tzset();

  rlimit stack_size;
  getrlimit(RLIMIT_STACK, &stack_size);
  stack_size.rlim_cur = 2 * 1024 * 1024;
  setrlimit(RLIMIT_STACK, &stack_size);

  starboard::InstallSuspendSignalHandlers();
  third_party::starboard::rdk::shared::InstallStopSignalHandlers();

#if SB_IS(EVERGREEN_COMPATIBLE)
  auto command_line = starboard::CommandLine(argc, argv);
  auto evergreen_content_path =
    command_line.GetSwitchValue(elf_loader::kEvergreenContent);
  std::string ca_certificates_path = evergreen_content_path.empty()
    ? starboard::GetCACertificatesPath()
    : starboard::GetCACertificatesPath(evergreen_content_path);
  if (ca_certificates_path.empty()) {
    SB_LOG(ERROR) << "Failed to get CA certificates path. Skip crashpad handler setup.";
  } else {
    crashpad::InstallCrashpadHandler(ca_certificates_path);
  }
#endif

  int result = 0;
  {
    result = SbRunStarboardMain(argc, argv, SbEventHandle);
  }

  third_party::starboard::rdk::shared::UninstallStopSignalHandlers();
  starboard::UninstallSuspendSignalHandlers();

  return result;
}

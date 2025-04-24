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

#include <malloc.h>
#include <time.h>

#include "starboard/configuration.h"
#include "starboard/event.h"
#include "starboard/shared/signal/crash_signals.h"
#include "starboard/shared/signal/debug_signals.h"
#include "starboard/shared/signal/suspend_signals.h"
#if SB_IS(EVERGREEN_COMPATIBLE)
#include "starboard/common/command_line.h"
#include "starboard/common/paths.h"
#include "starboard/elf_loader/elf_loader_constants.h"
#endif
#include "starboard/shared/x11/application_x11.h"

#include "starboard/crashpad_wrapper/wrapper.h"

extern "C" SB_EXPORT_PLATFORM int main(int argc, char** argv) {
  // Set M_ARENA_MAX to a low value to slow memory growth due to fragmentation.
  mallopt(M_ARENA_MAX, 2);

  tzset();
  starboard::shared::signal::InstallCrashSignalHandlers();
  starboard::shared::signal::InstallDebugSignalHandlers();
  starboard::shared::signal::InstallSuspendSignalHandlers();

#if SB_IS(EVERGREEN_COMPATIBLE)
  auto command_line = starboard::CommandLine(argc, argv);
  auto evergreen_content_path =
      command_line.GetSwitchValue(starboard::elf_loader::kEvergreenContent);
  std::string ca_certificates_path =
      evergreen_content_path.empty()
          ? starboard::common::GetCACertificatesPath()
          : starboard::common::GetCACertificatesPath(evergreen_content_path);
  if (ca_certificates_path.empty()) {
    SB_LOG(ERROR) << "Failed to get CA certificates path";
  }

#if !SB_IS(MODULAR)
  third_party::crashpad::wrapper::InstallCrashpadHandler(ca_certificates_path);
#endif  // !SB_IS(MODULAR)
#endif

#if SB_HAS_QUIRK(BACKTRACE_DLOPEN_BUG)
  // Call backtrace() once to work around potential
  // crash bugs in glibc, in dlopen()
  SbLogRawDumpStack(3);
#endif

  int result = SbRunStarboardMain(argc, argv, SbEventHandle);

  starboard::shared::signal::UninstallSuspendSignalHandlers();
  starboard::shared::signal::UninstallDebugSignalHandlers();
  starboard::shared::signal::UninstallCrashSignalHandlers();
  return result;
}

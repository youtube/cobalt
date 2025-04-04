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
#include "starboard/common/paths.h"
#include "starboard/elf_loader/elf_loader_constants.h"
#include "starboard/shared/starboard/command_line.h"
#endif
#include "starboard/shared/starboard/link_receiver.h"
#include "starboard/shared/x11/application_x11.h"

#include "third_party/crashpad/crashpad/wrapper/wrapper.h"

int preinit_starboard(int argc, char** argv) {
  // Set M_ARENA_MAX to a low value to slow memory growth due to fragmentation.
  mallopt(M_ARENA_MAX, 2);

  tzset();
  starboard::shared::signal::InstallCrashSignalHandlers();
  starboard::shared::signal::InstallDebugSignalHandlers();
  starboard::shared::signal::InstallSuspendSignalHandlers();

#if SB_IS(EVERGREEN_COMPATIBLE)
  auto command_line = starboard::shared::starboard::CommandLine(argc, argv);
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
  return 0;
}
int post_starboard() {
  starboard::shared::signal::UninstallSuspendSignalHandlers();
  starboard::shared::signal::UninstallDebugSignalHandlers();
  starboard::shared::signal::UninstallCrashSignalHandlers();
  return 0;
}

extern "C" SB_EXPORT_PLATFORM int main(int argc, char** argv) {
  preinit_starboard(argc, argv);
  // Previous code
  // int result = SbRunStarboardMain(argc, argv, SbEventHandle);
  int result = 0;

  // Code taken out from SbRunStarboardMain
  if (false) {
    // Current/Old version
    starboard::shared::x11::ApplicationX11 application(SbEventHandle);
    result = application.Run(argc, argv);
  } else {
    // non-blocking version, with loop owned by caller
    starboard::shared::x11::ApplicationX11 application(SbEventHandle, false);
    result = application.Run(argc, argv);

    // Loop taken out and broken
    application.RunLoop_Part0();
    // This does nothing but DispatchNextEvent()
    while (application.RunLoop_Part1()) {
    }
    application.RunLoop_Part2();
  }

  post_starboard();
  return result;
}

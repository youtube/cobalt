// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/event.h"

#include <malloc.h>
#include <time.h>

#include "build/build_config.h"
#include "starboard/configuration.h"
#include "starboard/crashpad_wrapper/wrapper.h"
#include "starboard/event.h"
#include "starboard/shared/signal/crash_signals.h"
#include "starboard/shared/signal/debug_signals.h"
#include "starboard/shared/signal/suspend_signals.h"
#include "starboard/shared/x11/application_x11.h"
#if SB_IS(EVERGREEN_COMPATIBLE)
#include "starboard/common/command_line.h"
#include "starboard/common/paths.h"
#include "starboard/elf_loader/elf_loader_constants.h"
#endif
#include "starboard/shared/x11/application_x11.h"

int SbRunStarboardMain(int argc, char** argv, SbEventHandleCallback callback) {
// The Crashpad client's dependency on //base, which is configured to use
// PartitionAlloc, is causing this call to mallopt to fail.
// TODO: b/406511608 - Cobalt: determine if we want to use PartitionAlloc, the
// allocator shim, etc. when //base is built with the starboard toolchain.
#if !BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)
  // Set M_ARENA_MAX to a low value to slow memory growth due to fragmentation.
  mallopt(M_ARENA_MAX, 2);
#endif  // !BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)

  tzset();
  starboard::InstallCrashSignalHandlers();
  starboard::InstallDebugSignalHandlers();
  starboard::InstallSuspendSignalHandlers();

#if SB_IS(EVERGREEN_COMPATIBLE)
  auto command_line = starboard::CommandLine(argc, argv);
  auto evergreen_content_path =
      command_line.GetSwitchValue(starboard::elf_loader::kEvergreenContent);
  std::string ca_certificates_path =
      evergreen_content_path.empty()
          ? starboard::GetCACertificatesPath()
          : starboard::GetCACertificatesPath(evergreen_content_path);
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

  starboard::ApplicationX11 application(callback);

  int result = application.Run(argc, argv);

  starboard::UninstallSuspendSignalHandlers();
  starboard::UninstallDebugSignalHandlers();
  starboard::UninstallCrashSignalHandlers();

  return result;
}

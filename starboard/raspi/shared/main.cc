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

#include <time.h>

#include "starboard/configuration.h"
#include "starboard/raspi/shared/application_dispmanx.h"
#include "starboard/shared/signal/crash_signals.h"
#include "starboard/shared/signal/suspend_signals.h"

#include "third_party/crashpad/wrapper/wrapper.h"

int main(int argc, char** argv) {
  tzset();

  starboard::shared::signal::InstallCrashSignalHandlers();
  starboard::shared::signal::InstallSuspendSignalHandlers();

#if SB_IS(EVERGREEN_COMPATIBLE)
  third_party::crashpad::wrapper::InstallCrashpadHandler();
#endif
  starboard::raspi::shared::ApplicationDispmanx application;
  int result = application.Run(argc, argv);
  starboard::shared::signal::UninstallSuspendSignalHandlers();
  starboard::shared::signal::UninstallCrashSignalHandlers();
  return result;
}

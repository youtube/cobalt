// Copyright 2016 Samsung Electronics. All Rights Reserved.
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

#include <aul.h>
#include <Elementary.h>

#include "starboard/configuration.h"
#include "starboard/shared/signal/crash_signals.h"
#include "starboard/shared/signal/suspend_signals.h"
#include "starboard/shared/wayland/application_wayland.h"

static Eina_Bool ecore_idle_cb(void* data) {
  if (!starboard::shared::wayland::ApplicationWayland::Get()->RunOnce()) {
    return ECORE_CALLBACK_CANCEL;
  }
  return ECORE_CALLBACK_RENEW;
}

static int aul_handler(aul_type type, bundle* kb, void* data) {
  static bool first_launch = true;
  switch (type) {
    case AUL_START: {
      SB_DLOG(INFO) << "AUL_START";
      if (first_launch) {
        starboard::shared::wayland::ApplicationWayland::Get()->RunReady(0,
                                                                        NULL);
        first_launch = false;
      } else {
        starboard::shared::wayland::ApplicationWayland::Get()->WindowRaise();
      }
      break;
    }
    case AUL_RESUME: {
      SB_DLOG(INFO) << "AUL_RESUME";
      break;
    }
    case AUL_TERMINATE: {
      SB_DLOG(INFO) << "AUL_TERMINATE";
      starboard::shared::wayland::ApplicationWayland::Get()->Stop(0);
      starboard::shared::wayland::ApplicationWayland::Get()->RunExit();
      elm_exit();
      break;
    }
    default:
      SB_DLOG(INFO) << "Unhandled aul event. type= " << type;
      break;
  }
  return 0;
}

int main(int argc, char** argv) {
  elm_init(0, NULL);

  aul_launch_init(aul_handler, NULL);
  aul_launch_argv_handler(argc, argv);

  starboard::shared::signal::InstallCrashSignalHandlers();
  starboard::shared::signal::InstallSuspendSignalHandlers();
  starboard::shared::wayland::ApplicationWayland application;
  ecore_idler_add(ecore_idle_cb, NULL);

  elm_run();
  elm_shutdown();

  starboard::shared::signal::UninstallSuspendSignalHandlers();
  starboard::shared::signal::UninstallCrashSignalHandlers();
  return 0;
}
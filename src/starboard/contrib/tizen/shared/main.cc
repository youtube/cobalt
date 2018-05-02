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
#include "starboard/shared/starboard/link_receiver.h"
#include "starboard/contrib/tizen/shared/wayland/application_tizen.h"

namespace {

int g_argc;
char** g_argv;

int aul_handler(aul_type type, bundle* kb, void* data) {
  switch (type) {
    case AUL_START: {
      SB_DLOG(INFO) << "AUL_START";
      starboard::shared::wayland::ApplicationWayland* app =
          starboard::shared::wayland::ApplicationWayland::Get();
      if (app) {
        starboard::shared::starboard::LinkReceiver receiver(app);
        app->Run(g_argc, g_argv);
      }
      break;
    }
    case AUL_RESUME: {
      SB_DLOG(INFO) << "AUL_RESUME";
      break;
    }
    case AUL_TERMINATE: {
      SB_DLOG(INFO) << "AUL_TERMINATE";
      starboard::shared::starboard::Application::Get()->Stop(0);
      elm_exit();
      break;
    }
    default:
      SB_DLOG(INFO) << "Unhandled aul event. type= " << type;
      break;
  }
  return 0;
}

}

int main(int argc, char** argv) {
  g_argc = argc;
  g_argv = argv;

  elm_init(0, NULL);

  aul_launch_init(aul_handler, NULL);
  aul_launch_argv_handler(argc, argv);

  starboard::shared::signal::InstallCrashSignalHandlers();
  starboard::shared::signal::InstallSuspendSignalHandlers();
  starboard::shared::wayland::ApplicationWaylandTizen application;

  elm_run();

  starboard::shared::signal::UninstallSuspendSignalHandlers();
  starboard::shared::signal::UninstallCrashSignalHandlers();
  elm_shutdown();
  return 0;
}

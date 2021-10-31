// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

// This is a test app for Evergreen that only works when it loads the
// evergreen-cert-test.html page.

#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/browser/application.h"
#include "cobalt/browser/switches.h"
#include "cobalt/version.h"
#include "starboard/event.h"
#include "starboard/loader_app/app_key.h"
#include "starboard/system.h"

namespace {

const char kMainAppKey[] = "aHR0cHM6Ly93d3cueW91dHViZS5jb20vdHY=";
// Use the html instead of url or app key to target the Evergreen cert test
// page, because there are various urls to launch the test page.
const char kEvergreenCertTestHtml[] = "evergreen-cert-test.html";

bool is_target_app = false;

cobalt::browser::Application* g_application = NULL;
bool g_is_startup_switch_set = false;

// Get the current version if the switch is set
bool CheckForAndExecuteVersionSwitch() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          cobalt::browser::switches::kVersion)) {
    SbLogRaw(base::StringPrintf("Cobalt version %s\n", COBALT_VERSION).c_str());
    return true;
  }
  return false;
}

// Get the help message if the switch is set
bool CheckForAndExecuteHelpSwitch() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          cobalt::browser::switches::kHelp)) {
    SbLogRaw("Options: \n");
    SbLogRaw(cobalt::browser::switches::HelpMessage().c_str());
    return true;
  }
  return false;
}

// If startup switches are set, the Cobalt browser application will not be
// initiallized and will exit early.
bool CheckForAndExecuteStartupSwitches() {
  if (CheckForAndExecuteVersionSwitch()) g_is_startup_switch_set = true;
  if (CheckForAndExecuteHelpSwitch()) g_is_startup_switch_set = true;
  return g_is_startup_switch_set;
}

void PreloadApplication(int argc, char** argv, const char* link,
                        const base::Closure& quit_closure,
                        SbTimeMonotonic timestamp) {
  if (CheckForAndExecuteStartupSwitches()) {
    SbSystemRequestStop(0);
    return;
  }
  LOG(INFO) << "Concealing application.";
  DCHECK(!g_application);
  g_application = new cobalt::browser::Application(
      quit_closure, true /*should_preload*/, timestamp);
  DCHECK(g_application);
}

void StartApplication(int argc, char** argv, const char* link,
                      const base::Closure& quit_closure,
                      SbTimeMonotonic timestamp) {
  if (CheckForAndExecuteStartupSwitches()) {
    SbSystemRequestStop(0);
    return;
  }
  LOG(INFO) << "Starting application.";
#if SB_API_VERSION >= 13
  DCHECK(!g_application);
  g_application = new cobalt::browser::Application(
      quit_closure, false /*not_preload*/, timestamp);
  DCHECK(g_application);
#else
  if (!g_application) {
    g_application = new cobalt::browser::Application(
        quit_closure, false /*should_preload*/, timestamp);
    DCHECK(g_application);
  } else {
    g_application->Start(timestamp);
  }
#endif  // SB_API_VERSION >= 13
}

void StopApplication() {
  LOG(INFO) << "Stopping application.";
  delete g_application;
  g_application = NULL;
}

void HandleStarboardEvent(const SbEvent* starboard_event) {
  DCHECK(starboard_event);
  if (g_application) {
    g_application->HandleStarboardEvent(starboard_event);
  } else if (!g_is_startup_switch_set) {
    LOG(ERROR) << "Unable to handle starboard event because there is no "
                  "existing application!";
  }
}

}  // namespace

void SbEventHandle(const SbEvent* event) {
  if (is_target_app)
    return ::cobalt::wrap_main::BaseEventHandler<
        PreloadApplication, StartApplication, HandleStarboardEvent,
        StopApplication>(event);

  const SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
  const base::CommandLine command_line(
      data->argument_count, const_cast<const char**>(data->argument_values));

  if (command_line.HasSwitch(cobalt::browser::switches::kInitialURL)) {
    std::string url = command_line.GetSwitchValueASCII(
        cobalt::browser::switches::kInitialURL);
    if (starboard::loader_app::GetAppKey(url) != kMainAppKey &&
        url.find(kEvergreenCertTestHtml) == std::string::npos) {
      // If the app is not the main app nor Evergreen cert test page, stop the
      // app.
      SbSystemRequestStop(0);
    }
  }

  // If the url is the main app or the Evergreen cert test page, hook up the app
  // lifecycle functions.
  is_target_app = true;
  return ::cobalt::wrap_main::BaseEventHandler<
      PreloadApplication, StartApplication, HandleStarboardEvent,
      StopApplication>(event);
}

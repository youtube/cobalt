// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/lifecycle_console_commands.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "starboard/system.h"

namespace cobalt {
namespace browser {

const char kPauseCommand[] = "pause";
const char kPauseCommandShortHelp[] = "Sends a request to pause Cobalt.";
const char kPauseCommandLongHelp[] =
    "Sends a request to the platform to pause Cobalt, indicating that it is "
    "not in focus and sending a blur event to the web application.";

const char kUnpauseCommand[] = "unpause";
const char kUnpauseCommandShortHelp[] = "Sends a request to unpause Cobalt.";
const char kUnpauseCommandLongHelp[] =
    "Sends a request to the platform to unpause Cobalt, resulting in a focus "
    "event being sent to the web application.";

const char kSuspendCommand[] = "suspend";
const char kSuspendCommandShortHelp[] = "Sends a request to suspend Cobalt.";
const char kSuspendCommandLongHelp[] =
    "Sends a request to the platform to suspend Cobalt, indicating that it is "
    "not visible and sending a hidden event to the web application.  Note that "
    "Cobalt may become unresponsive after this call and you will need to "
    "resume it in a platform-specific way.";

const char kQuitCommand[] = "quit";
const char kQuitCommandShortHelp[] = "Sends a request to stop Cobalt.";
const char kQuitCommandLongHelp[] =
    "Sends a request to the platform to stop Cobalt, shutting it down and "
    "ending the process (peacefully).";

namespace {
#if SB_API_VERSION >= 13
// This is temporary that will be changed in later CLs, for mapping Starboard
// Concealed state support onto Cobalt without Concealed state support to be
// able to test the former.
void OnPause(const std::string& /*message*/) { SbSystemRequestBlur(); }

void OnUnpause(const std::string& /*message*/) { SbSystemRequestFocus(); }

void OnSuspend(const std::string& /*message*/) {
  LOG(INFO) << "Concealing Cobalt through the console, but you will need to "
            << "reveal Cobalt using a platform-specific method.";
  SbSystemRequestConceal();
}
#else
void OnPause(const std::string& message) { SbSystemRequestPause(); }

void OnUnpause(const std::string& message) { SbSystemRequestUnpause(); }

void OnSuspend(const std::string& message) {
  LOG(INFO) << "Suspending Cobalt through the console, but you will need to "
            << "resume Cobalt using a platform-specific method.";
  SbSystemRequestSuspend();
}
#endif  // SB_API_VERSION >= 13

void OnQuit(const std::string& /*message*/) { SbSystemRequestStop(0); }
}  // namespace

LifecycleConsoleCommands::LifecycleConsoleCommands()
    : pause_command_handler_(kPauseCommand, base::Bind(&OnPause),
                             kPauseCommandShortHelp, kPauseCommandLongHelp),
      unpause_command_handler_(kUnpauseCommand, base::Bind(OnUnpause),
                               kUnpauseCommandShortHelp,
                               kUnpauseCommandLongHelp),
      suspend_command_handler_(kSuspendCommand, base::Bind(OnSuspend),
                               kSuspendCommandShortHelp,
                               kSuspendCommandLongHelp),
      quit_command_handler_(kQuitCommand, base::Bind(OnQuit),
                            kQuitCommandShortHelp, kQuitCommandLongHelp) {}

}  // namespace browser
}  // namespace cobalt

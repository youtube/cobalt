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

const char kBlurCommand[] = "blur";
const char kBlurCommandShortHelp[] = "Sends a request to blur Cobalt.";
const char kBlurCommandLongHelp[] =
    "Sends a request to the platform to blur Cobalt, indicating that it is "
    "not in focus and sending a blur event to the web application.";

const char kFocusCommand[] = "focus";
const char kFocusCommandShortHelp[] = "Sends a request to focus Cobalt.";
const char kFocusCommandLongHelp[] =
    "Sends a request to the platform to focus Cobalt, resulting in a Focus "
    "event being sent to the web application.";

const char kConcealCommand[] = "conceal";
const char kConcealCommandShortHelp[] = "Sends a request to conceal Cobalt.";
const char kConcealCommandLongHelp[] =
    "Sends a request to the platform to conceal Cobalt, indicating that it is "
    "not visible and sending a hidden event to the web application.  Note that "
    "Cobalt may become unresponsive after this call in which case you will "
    "need to resume it in a platform-specific way.";

const char kFreezeCommand[] = "freeze";
const char kFreezeCommandShortHelp[] = "Sends a request to freeze Cobalt.";
const char kFreezeCommandLongHelp[] =
    "Sends a request to the platform to freeze Cobalt, indicating that it is "
    "not visible, sending a hidden event to the web application, and halt "
    "processing.  Note that Cobalt may become unresponsive after this call in "
    "which case you will need to resume it in a platform-specific way.";

const char kRevealCommand[] = "reveal";
const char kRevealCommandShortHelp[] = "Sends a request to reveal Cobalt.";
const char kRevealCommandLongHelp[] =
    "Sends a request to the platform to reveal Cobalt, indicating that it is "
    "visible but not focues, and sending a visible event to the web "
    "application.  Note that "
    "Cobalt may be unresponsive in which case you will need to "
    "resume it in a platform-specific way.";

const char kQuitCommand[] = "quit";
const char kQuitCommandShortHelp[] = "Sends a request to stop Cobalt.";
const char kQuitCommandLongHelp[] =
    "Sends a request to the platform to stop Cobalt, shutting it down and "
    "ending the process (peacefully).";

namespace {
void OnBlur(const std::string&) { SbSystemRequestBlur(); }
void OnFocus(const std::string&) { SbSystemRequestFocus(); }
void OnConceal(const std::string&) {
  LOG(WARNING)
      << "Concealing Cobalt through the console.  Note that Cobalt may "
         "become unresponsive after this call in which case you will "
         "need to resume it in a platform-specific way.";
  SbSystemRequestConceal();
}
void OnFreeze(const std::string&) {
  LOG(WARNING) << "Freezing Cobalt through the console.  Note that Cobalt may "
                  "become unresponsive after this call in which case you will "
                  "need to resume it in a platform-specific way.";
  SbSystemRequestFreeze();
}
void OnReveal(const std::string&) { SbSystemRequestReveal(); }
void OnQuit(const std::string& /*message*/) { SbSystemRequestStop(0); }
}  // namespace

LifecycleConsoleCommands::LifecycleConsoleCommands()
    : blur_command_handler_(kBlurCommand, base::Bind(OnBlur),
                            kBlurCommandShortHelp, kBlurCommandLongHelp),
      focus_command_handler_(kFocusCommand, base::Bind(OnFocus),
                             kFocusCommandShortHelp, kFocusCommandLongHelp),
      conceal_command_handler_(kConcealCommand, base::Bind(OnConceal),
                               kConcealCommandShortHelp,
                               kConcealCommandLongHelp),
      freeze_command_handler_(kFreezeCommand, base::Bind(OnFreeze),
                              kFreezeCommandShortHelp, kFreezeCommandLongHelp),
      reveal_command_handler_(kRevealCommand, base::Bind(OnReveal),
                              kRevealCommandShortHelp, kRevealCommandLongHelp),
      quit_command_handler_(kQuitCommand, base::Bind(OnQuit),
                            kQuitCommandShortHelp, kQuitCommandLongHelp) {}

}  // namespace browser
}  // namespace cobalt

// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/debug_console.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "cobalt/base/cobalt_paths.h"
#include "cobalt/base/source_location.h"
#include "cobalt/browser/switches.h"
#include "cobalt/dom/csp_delegate_factory.h"
#include "cobalt/dom/mutation_observer_task_manager.h"
#include "cobalt/dom/window.h"

namespace cobalt {
namespace browser {

namespace {
// Files for the debug console web page are bundled with the executable.
const char kInitialDebugConsoleUrl[] =
    "file:///cobalt/debug/console/debug_console.html";

const char kDebugConsoleOffString[] = "off";
const char kDebugConsoleOnString[] = "on";
const char kDebugConsoleHudString[] = "hud";

// Convert from a debug console visibility setting string to an integer
// value specified by a constant defined in debug::console::DebugHub.
base::Optional<int> DebugConsoleModeStringToInt(
    const std::string& mode_string) {
  // Static casting is necessary in order to get around what appears to be a
  // compiler error on Linux when implicitly constructing a base::Optional<int>
  // from a static const int.
  if (mode_string == kDebugConsoleOffString) {
    return static_cast<int>(debug::console::DebugHub::kDebugConsoleOff);
  } else if (mode_string == kDebugConsoleHudString) {
    return static_cast<int>(debug::console::DebugHub::kDebugConsoleHud);
  } else if (mode_string == kDebugConsoleOnString) {
    return static_cast<int>(debug::console::DebugHub::kDebugConsoleOn);
  } else {
    DLOG(WARNING) << "Debug console mode \"" << mode_string
                  << "\" not recognized.";
    return base::nullopt;
  }
}

// Returns the debug console mode as specified by the command line.
// If unspecified by the command line, base::nullopt is returned.
base::Optional<int> GetDebugConsoleModeFromCommandLine() {
#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kDebugConsoleMode)) {
    const std::string debug_console_mode_string =
        command_line->GetSwitchValueASCII(switches::kDebugConsoleMode);
    return DebugConsoleModeStringToInt(debug_console_mode_string);
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  return base::nullopt;
}

// Returns the debug console's initial visibility mode.
int GetInitialMode() {
  // First check to see if the mode is explicitly set from the command line.
  base::Optional<int> mode_from_command_line =
      GetDebugConsoleModeFromCommandLine();
  if (mode_from_command_line) {
    return *mode_from_command_line;
  }

  // By default the debug console is off.
  return debug::console::DebugHub::kDebugConsoleOff;
}

// A function to create a DebugHub object, to be injected into WebModule.
scoped_refptr<script::Wrappable> CreateDebugHub(
    const debug::console::DebugHub::GetHudModeCallback& get_hud_mode_function,
    const debug::CreateDebugClientCallback& create_debug_client_callback,
    const scoped_refptr<dom::Window>& window,
    dom::MutationObserverTaskManager* mutation_observer_task_manager,
    script::GlobalEnvironment* global_environment) {
  SB_UNREFERENCED_PARAMETER(window);
  SB_UNREFERENCED_PARAMETER(mutation_observer_task_manager);
  SB_UNREFERENCED_PARAMETER(global_environment);
  return new debug::console::DebugHub(get_hud_mode_function,
                                      create_debug_client_callback);
}

}  // namespace

DebugConsole::DebugConsole(
    base::ApplicationState initial_application_state,
    const WebModule::OnRenderTreeProducedCallback&
        render_tree_produced_callback,
    network::NetworkModule* network_module,
    const cssom::ViewportSize& window_dimensions,
    render_tree::ResourceProvider* resource_provider, float layout_refresh_rate,
    const debug::CreateDebugClientCallback& create_debug_client_callback) {
  mode_ = GetInitialMode();

  WebModule::Options web_module_options;
  web_module_options.name = "DebugConsoleWebModule";
  // The debug console does not load any image assets.
  web_module_options.image_cache_capacity = 0;
  // Disable CSP for the Debugger's WebModule. This will also allow eval() in
  // javascript.
  web_module_options.csp_enforcement_mode = dom::kCspEnforcementDisable;
  web_module_options.csp_insecure_allowed_token =
      dom::CspDelegateFactory::GetInsecureAllowedToken();

  // Since the debug console is intended to be overlaid on top of the main
  // web module contents, make sure blending is enabled for its background.
  web_module_options.clear_window_with_background_color = false;

  // Attach a DebugHub object to the "debugHub" Window attribute for this
  // web module so that JavaScript within this WebModule has access to DebugHub
  // functionality.
  web_module_options.injected_window_attributes["debugHub"] =
      base::Bind(&CreateDebugHub,
                 base::Bind(&DebugConsole::GetMode, base::Unretained(this)),
                 create_debug_client_callback);
  web_module_.reset(new WebModule(
      GURL(kInitialDebugConsoleUrl), initial_application_state,
      render_tree_produced_callback,
      base::Bind(&DebugConsole::OnError, base::Unretained(this)),
      WebModule::CloseCallback(), /* window_close_callback */
      base::Closure(),            /* window_minimize_callback */
      NULL /* can_play_type_handler */, NULL /* web_media_player_factory */,
      network_module, window_dimensions, 1.f /*video_pixel_ratio*/,
      resource_provider, layout_refresh_rate, web_module_options));
}

DebugConsole::~DebugConsole() {}

bool DebugConsole::FilterKeyEvent(base::Token type,
                                  const dom::KeyboardEventInit& event) {
  // Assume here the full debug console is visible - pass all events to its
  // web module, and return false to indicate the event has been consumed.
  web_module_->InjectKeyboardEvent(type, event);
  return false;
}

bool DebugConsole::FilterWheelEvent(base::Token type,
                                    const dom::WheelEventInit& event) {
  // Assume here the full debug console is visible - pass all events to its
  // web module, and return false to indicate the event has been consumed.
  web_module_->InjectWheelEvent(type, event);
  return false;
}

bool DebugConsole::FilterPointerEvent(base::Token type,
                                      const dom::PointerEventInit& event) {
  // Assume here the full debug console is visible - pass all events to its
  // web module, and return false to indicate the event has been consumed.
  web_module_->InjectPointerEvent(type, event);
  return false;
}

#if SB_API_VERSION >= SB_ON_SCREEN_KEYBOARD_REQUIRED_VERSION || \
    SB_HAS(ON_SCREEN_KEYBOARD)
bool DebugConsole::InjectOnScreenKeyboardInputEvent(
    base::Token type, const dom::InputEventInit& event) {
  // Assume here the full debug console is visible - pass all events to its
  // web module, and return false to indicate the event has been consumed.
  web_module_->InjectOnScreenKeyboardInputEvent(type, event);
  return false;
}
#endif  // SB_API_VERSION >= SB_ON_SCREEN_KEYBOARD_REQUIRED_VERSION ||
        // SB_HAS(ON_SCREEN_KEYBOARD)

void DebugConsole::SetMode(int mode) {
  base::AutoLock lock(mode_mutex_);
  mode_ = mode;
}

void DebugConsole::CycleMode() {
  base::AutoLock lock(mode_mutex_);
  mode_ = (mode_ + 1) % debug::console::DebugHub::kDebugConsoleNumModes;
}

int DebugConsole::GetMode() {
  base::AutoLock lock(mode_mutex_);
  return mode_;
}

}  // namespace browser
}  // namespace cobalt

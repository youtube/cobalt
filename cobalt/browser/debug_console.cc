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
    "file:///debug_console/debug_console.html";

const char kDebugConsoleModeOffString[] = "off";
const char kDebugConsoleModeHudString[] = "hud";
const char kDebugConsoleModeDebugString[] = "debug";
const char kDebugConsoleModeDebugStringAlias[] = "on";  // Legacy name of mode.
const char kDebugConsoleModeMediaString[] = "media";

// Convert from a debug console visibility setting string to an integer
// value specified by a constant defined in debug::console::DebugHub.
base::Optional<int> DebugConsoleModeStringToInt(
    const std::string& mode_string) {
  // Static casting is necessary in order to get around what appears to be a
  // compiler error on Linux when implicitly constructing a base::Optional<int>
  // from a static const int.
  if (mode_string == kDebugConsoleModeOffString) {
    return static_cast<int>(debug::console::kDebugConsoleModeOff);
  } else if (mode_string == kDebugConsoleModeHudString) {
    return static_cast<int>(debug::console::kDebugConsoleModeHud);
  } else if (mode_string == kDebugConsoleModeDebugString) {
    return static_cast<int>(debug::console::kDebugConsoleModeDebug);
  } else if (mode_string == kDebugConsoleModeDebugStringAlias) {
    return static_cast<int>(debug::console::kDebugConsoleModeDebug);
  } else if (mode_string == kDebugConsoleModeMediaString) {
    return static_cast<int>(debug::console::kDebugConsoleModeMedia);
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
  return debug::console::kDebugConsoleModeOff;
}

// A function to create a DebugHub object, to be injected into WebModule.
scoped_refptr<script::Wrappable> CreateDebugHub(
    const debug::console::DebugHub::GetHudModeCallback& get_hud_mode_function,
    const debug::CreateDebugClientCallback& create_debug_client_callback,
    const scoped_refptr<dom::Window>& window,
    script::GlobalEnvironment* global_environment) {
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
    const debug::CreateDebugClientCallback& create_debug_client_callback,
    const base::Closure& maybe_freeze_callback) {
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

  // Pass down this callback from Browser module to Web module eventually.
  web_module_options.maybe_freeze_callback = maybe_freeze_callback;

  web_module_.reset(new WebModule(
      GURL(kInitialDebugConsoleUrl), initial_application_state,
      render_tree_produced_callback,
      base::Bind(&DebugConsole::OnError, base::Unretained(this)),
      WebModule::CloseCallback(), /* window_close_callback */
      base::Closure(),            /* window_minimize_callback */
      NULL /* can_play_type_handler */, NULL /* media_module */,
      network_module, window_dimensions, resource_provider, layout_refresh_rate,
      web_module_options));
}

DebugConsole::~DebugConsole() {}

bool DebugConsole::ShouldInjectInputEvents() {
  switch (GetMode()) {
    case debug::console::kDebugConsoleModeOff:
    case debug::console::kDebugConsoleModeHud:
      return false;
    default:
      return true;
  }
}

bool DebugConsole::FilterKeyEvent(base::Token type,
                                  const dom::KeyboardEventInit& event) {
  // Return true to indicate the event should still be handled.
  if (!ShouldInjectInputEvents()) return true;

  web_module_->InjectKeyboardEvent(type, event);
  return false;
}

bool DebugConsole::FilterWheelEvent(base::Token type,
                                    const dom::WheelEventInit& event) {
  // Return true to indicate the event should still be handled.
  if (!ShouldInjectInputEvents()) return true;

  web_module_->InjectWheelEvent(type, event);
  return false;
}

bool DebugConsole::FilterPointerEvent(base::Token type,
                                      const dom::PointerEventInit& event) {
  // Return true to indicate the event should still be handled.
  if (!ShouldInjectInputEvents()) return true;

  web_module_->InjectPointerEvent(type, event);
  return false;
}

#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
bool DebugConsole::FilterOnScreenKeyboardInputEvent(
    base::Token type, const dom::InputEventInit& event) {
  // Return true to indicate the event should still be handled.
  if (!ShouldInjectInputEvents()) return true;

  web_module_->InjectOnScreenKeyboardInputEvent(type, event);
  return false;
}
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)

void DebugConsole::CycleMode() {
  base::AutoLock lock(mode_mutex_);
  mode_ = (mode_ + 1) % debug::console::kDebugConsoleModeCount;
}

debug::console::DebugConsoleMode DebugConsole::GetMode() {
  base::AutoLock lock(mode_mutex_);
  return static_cast<debug::console::DebugConsoleMode>(mode_);
}

}  // namespace browser
}  // namespace cobalt

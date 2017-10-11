// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_DEBUG_CONSOLE_H_
#define COBALT_BROWSER_DEBUG_CONSOLE_H_

#if defined(ENABLE_DEBUG_CONSOLE)

#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "cobalt/base/token.h"
#include "cobalt/browser/lifecycle_observer.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/debug/debug_hub.h"
#include "cobalt/dom/input_event_init.h"
#include "cobalt/dom/keyboard_event_init.h"
#include "cobalt/dom/window.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace browser {

// DebugConsole wraps the web module and all components used to implement the
// debug console.
class DebugConsole : public LifecycleObserver {
 public:
  DebugConsole(
      base::ApplicationState initial_application_state,
      const WebModule::OnRenderTreeProducedCallback&
          render_tree_produced_callback,
      network::NetworkModule* network_module,
      const math::Size& window_dimensions,
      render_tree::ResourceProvider* resource_provider,
      float layout_refresh_rate,
      const debug::Debugger::GetDebugServerCallback& get_debug_server_callback,
      const script::JavaScriptEngine::Options& javascript_engine_options,
      const dom::Window::GetSbWindowCallback& get_sb_window_callback);
  ~DebugConsole();

  // Filters a key event.
  // Returns true if the event should be passed on to other handlers,
  // false if it was consumed within this function.
  bool FilterKeyEvent(base::Token type, const dom::KeyboardEventInit& event);

#if SB_HAS(ON_SCREEN_KEYBOARD)
  // Inject an on screen keyboard input event.
  // Returns true if the event should be passed on to other handlers,
  // false if it was consumed within this function.
  bool InjectOnScreenKeyboardInputEvent(base::Token type,
                                        const dom::InputEventInit& event);
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

  const WebModule& web_module() const { return *web_module_; }
  WebModule& web_module() { return *web_module_; }

  // Sets the debug console's visibility mode.
  void SetMode(int mode);
  // Cycles through each different possible debug console visibility mode.
  void CycleMode();
  // Returns the currently set debug console visibility mode.
  int GetMode();

  void SetSize(const math::Size& window_dimensions, float video_pixel_ratio) {
    web_module_->SetSize(window_dimensions, video_pixel_ratio);
  }

  // LifecycleObserver implementation.
  void Prestart() OVERRIDE { web_module_->Prestart(); }
  void Start(render_tree::ResourceProvider* resource_provider) OVERRIDE {
    web_module_->Start(resource_provider);
  }
  void Pause() OVERRIDE { web_module_->Pause(); }
  void Unpause() OVERRIDE { web_module_->Unpause(); }
  void Suspend() OVERRIDE { web_module_->Suspend(); }
  void Resume(render_tree::ResourceProvider* resource_provider) OVERRIDE {
    web_module_->Resume(resource_provider);
  }

  void ReduceMemory() { web_module_->ReduceMemory(); }

 private:
  void OnError(const GURL& /* url */, const std::string& error) {
    LOG(ERROR) << error;
  }

  // The current console visibility mode.  The mutex is required since the debug
  // console's visibility mode may be accessed from both the WebModule thread
  // and the DebugConsole's host thread.
  base::Lock mode_mutex_;
  int mode_;

  // Sets up everything to do with the management of the web page that
  // implements the debug console.
  // This web module will produce a second render tree to combine with the main
  // one.
  scoped_ptr<WebModule> web_module_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // ENABLE_DEBUG_CONSOLE
#endif  // COBALT_BROWSER_DEBUG_CONSOLE_H_

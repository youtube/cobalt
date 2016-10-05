/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_BROWSER_DEBUG_CONSOLE_H_
#define COBALT_BROWSER_DEBUG_CONSOLE_H_

#if defined(ENABLE_DEBUG_CONSOLE)

#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/debug/debug_hub.h"
#include "cobalt/dom/keyboard_event.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace browser {

// DebugConsole wraps the web module and all components used to implement the
// debug console.
class DebugConsole {
 public:
  DebugConsole(
      const WebModule::OnRenderTreeProducedCallback&
          render_tree_produced_callback,
      media::MediaModule* media_module, network::NetworkModule* network_module,
      const math::Size& window_dimensions,
      render_tree::ResourceProvider* resource_provider,
      float layout_refresh_rate,
      const debug::Debugger::GetDebugServerCallback& get_debug_server_callback);
  ~DebugConsole();

  // Filters a key event.
  // Returns true if the event should be passed on to other handlers,
  // false if it was consumed within this function.
  bool FilterKeyEvent(const dom::KeyboardEvent::Data& event);

  const WebModule& web_module() const { return *web_module_; }
  WebModule& web_module() { return *web_module_; }

  // Sets the debug console's visibility mode.
  void SetMode(int mode);
  // Cycles through each different possible debug console visibility mode.
  void CycleMode();
  // Returns the currently set debug console visibility mode.
  int GetMode();

  void Suspend();
  void Resume(render_tree::ResourceProvider* resource_provider);

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

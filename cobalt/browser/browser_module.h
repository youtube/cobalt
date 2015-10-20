/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef BROWSER_BROWSER_MODULE_H_
#define BROWSER_BROWSER_MODULE_H_

#include <string>

#include "cobalt/browser/debug_console.h"
#include "cobalt/browser/render_tree_combiner.h"
#include "cobalt/browser/web_module.h"
#include "cobalt/debug/debug_hub.h"
#include "cobalt/dom/keyboard_event.h"
#include "cobalt/h5vcc/h5vcc.h"
#include "cobalt/input/input_device_manager.h"
#include "cobalt/layout/layout_manager.h"
#include "cobalt/network/network_module.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/storage/storage_manager.h"
#include "cobalt/system_window/create_system_window.h"
#include "cobalt/trace_event/scoped_trace_to_file.h"

namespace cobalt {
namespace browser {

// BrowserModule hosts all major components of the Cobalt browser application.
// It also contains all of the glue components required to connect the
// different subsystems together.
class BrowserModule {
 public:
  // All browser subcomponent options should have default constructors that
  // setup reasonable default options.
  struct Options {
    renderer::RendererModule::Options renderer_module_options;
    storage::StorageManager::Options storage_manager_options;
    WebModule::Options web_module_options;
    std::string language;
  };

  BrowserModule(const GURL& url, const Options& options);
  ~BrowserModule();

  const std::string& GetUserAgent() { return network_module_.user_agent(); }

 private:
  // Glue function to deal with the production of the main render tree,
  // and will manage handing it off to the renderer.
  void OnRenderTreeProduced(
      const browser::WebModule::LayoutResults& layout_results);

  // Glue function to deal with the production of the debug console render tree,
  // and will manage handing it off to the renderer.
  void OnDebugConsoleRenderTreeProduced(
      const browser::WebModule::LayoutResults& layout_results);

  // Sets the debug console mode if specified on the command-line.
  // Returns true if the debug console mode was set, false otherwise.
  bool SetDebugConsoleModeFromCommandLine();

  // Saves/loads the debug console mode to/from local storage so we can
  // persist the user's preference.
  void SaveDebugConsoleMode();
  void LoadDebugConsoleMode();

  // Glue function to deal with the production of an input event from the
  // input device, and manage handing it off to the web module for
  // interpretation.
  void OnKeyEventProduced(const scoped_refptr<dom::KeyboardEvent>& event);

  // Error callback for any error that stops the program.
  void OnError(const std::string& error) { LOG(ERROR) << error; }

  // Filters a key event.
  // Returns true if the event should be passed on to other handlers,
  // false if it was consumed within this function.
  bool FilterKeyEvent(const scoped_refptr<dom::KeyboardEvent>& event);

  // Filters a key event for hotkeys.
  // Returns true if the event should be passed on to other handlers,
  // false if it was consumed within this function.
  bool FilterKeyEventForHotkeys(const scoped_refptr<dom::KeyboardEvent>& event);

  // The main system window for our browser.
  // This routes event callbacks, and provides a native window handle
  // on desktop systems.
  scoped_ptr<system_window::SystemWindow> main_system_window_;

  storage::StorageManager storage_manager_;

  // Sets up everything to do with graphics, from backend objects like the
  // display and graphics context to the rasterizer and rendering pipeline.
  renderer::RendererModule renderer_module_;

  // Controls all media playback related objects/resources.
  scoped_ptr<media::MediaModule> media_module_;

  // Sets up the network component for requesting internet resources.
  network::NetworkModule network_module_;

  // Manages the two render trees, combines and renders them.
  RenderTreeCombiner render_tree_combiner_;

  // Sets up everything to do with web page management, from loading and
  // parsing the web page and all referenced files to laying it out.  The
  // web module will ultimately produce a render tree that can be passed
  // into the renderer module.
  WebModule web_module_;

  // Manages debug communication with the web modules.
  scoped_refptr<debug::DebugHub> debug_hub_;

  // Manages a second web module to implement the debug console.
  DebugConsole debug_console_;

  // The browser module runs on this message loop.
  MessageLoop* const self_message_loop_;

  // Wraps input device and produces input events that can be passed into
  // the web module.
  scoped_ptr<input::InputDeviceManager> input_device_manager_;

  // This object can be set to start a trace if a hotkey (like F3) is pressed.
  // While initialized, it means that a trace is on-going.
  scoped_ptr<trace_event::ScopedTraceToFile> trace_to_file_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // BROWSER_BROWSER_MODULE_H_

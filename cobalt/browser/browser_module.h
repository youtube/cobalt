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
#include "cobalt/input/input_device_manager.h"
#include "cobalt/layout/layout_manager.h"
#include "cobalt/network/network_module.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/storage/storage_manager.h"

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
  };

  explicit BrowserModule(const Options& options);
  ~BrowserModule();

  std::string GetUserAgent() { return web_module_.GetUserAgent(); }

 private:
  // Glue function to deal with the production of the main render tree,
  // and will manage handing it off to the renderer.
  void OnRenderTreeProduced(
      const scoped_refptr<render_tree::Node>& render_tree,
      const scoped_refptr<render_tree::animations::NodeAnimationsMap>&
          node_animations_map);

  // Glue function to deal with the production of the debug console render tree,
  // and will manage handing it off to the renderer.
  void OnDebugConsoleRenderTreeProduced(
      const scoped_refptr<render_tree::Node>& render_tree,
      const scoped_refptr<render_tree::animations::NodeAnimationsMap>&
          node_animations_map);

  // Glue function to deal with the production of an input event from the
  // input device, and manage handing it off to the web module for
  // interpretation.
  void OnKeyEventProduced(const scoped_refptr<dom::KeyboardEvent>& event);

  // Error callback for any error that stops the program.
  void OnError(const std::string& error) { LOG(ERROR) << error; }

  storage::StorageManager storage_manager_;

  // Sets up everything to do with graphics, from backend objects like the
  // display and graphics context to the rasterizer and rendering pipeline.
  renderer::RendererModule renderer_module_;

  // Controls all media playback related objects/resources.
  scoped_ptr<media::MediaModule> media_module_;

  // Sets up the network component for requesting internet resources.
  network::NetworkModule network_module_;

  // Manages debug communication with the web modules.
  scoped_refptr<debug::DebugHub> debug_hub_;

  // Manages a second web module to implement the debug console.
  DebugConsole debug_console_;

  // Manages the two render trees, combines and renders them.
  RenderTreeCombiner render_tree_combiner_;

  // Sets up everything to do with web page management, from loading and
  // parsing the web page and all referenced files to laying it out.  The
  // web module will ultimately produce a render tree that can be passed
  // into the renderer module.
  WebModule web_module_;

  // The browser module runs on this message loop.
  MessageLoop* const browser_module_message_loop_;

  // Wraps input device and produces input events that can be passed into
  // the web module.
  scoped_ptr<input::InputDeviceManager> input_device_manager_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // BROWSER_BROWSER_MODULE_H_

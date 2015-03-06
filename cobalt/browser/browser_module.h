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

#include "cobalt/browser/input_device_adapter.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/dom/window.h"
#include "cobalt/layout/layout_manager.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/renderer/renderer_module.h"
#include "cobalt/script/global_object_proxy.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/script/script_runner.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace browser {

// BrowserModule hosts all major components of the Cobalt browser application.
// It also contains all of the glue components required to connect the
// different subsystems together.  By constructing a BrowserModule object in
// platform specific code, platform-specific options can be specified.
class BrowserModule {
 public:
  // All browser subcomponent options should have default constructors that
  // setup reasonable default options.
  struct Options {
    renderer::RendererModule::Options renderer_module_options;
    GURL url;
  };

  BrowserModule(const std::string& user_agent, const Options& options);
  ~BrowserModule();

  renderer::RendererModule* renderer_module() { return &renderer_module_; }

 private:
  // Glue function to deal with the production of a render tree, and will
  // manage handing it off to the renderer.
  void OnRenderTreeProduced(
      const scoped_refptr<render_tree::Node>& render_tree);

  // Sets up everything to do with graphics, from backend objects like the
  // display and graphics context to the rasterizer and rendering pipeline.
  renderer::RendererModule renderer_module_;

  scoped_ptr<css_parser::Parser> css_parser_;

  // JavaScript engine for the browser.
  scoped_ptr<script::JavaScriptEngine> javascript_engine_;

  // JavaScript Global Object for the browser. There should be one per window,
  // but since there is only one window, we can have one per browser.
  scoped_refptr<script::GlobalObjectProxy> global_object_proxy_;

  // Interface for the document to execute JavaScript code.
  scoped_ptr<script::ScriptRunner> script_runner_;

  // FetcherFactory that is used to create a fetcher according to URL.
  scoped_ptr<loader::FetcherFactory> fetcher_factory_;

  // The Window object wraps all DOM-related components.
  scoped_refptr<dom::Window> window_;

  // Triggers layout whenever the document changes.
  layout::LayoutManager layout_manager_;

  // Wraps input device and dom interaction logic.
  InputDeviceAdapter input_device_adapter_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // BROWSER_BROWSER_MODULE_H_

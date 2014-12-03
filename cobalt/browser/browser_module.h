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

#include "cobalt/browser/dummy_render_tree_source.h"
#include "cobalt/renderer/renderer_module.h"

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
  };

  explicit BrowserModule(const Options& options);
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

  // Until the html/css parser and layout engine are in place and functional,
  // we use a DummyRenderTreeSource object to feed the graphics pipeline.
  DummyRenderTreeSource dummy_render_tree_source_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // BROWSER_BROWSER_MODULE_H_

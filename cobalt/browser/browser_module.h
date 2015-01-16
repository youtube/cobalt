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

#include "cobalt/browser/dom/document.h"
#include "cobalt/browser/dom/document_builder.h"
#include "cobalt/browser/dom/document_builder.h"
#include "cobalt/browser/dom/html_element_factory.h"
#include "cobalt/browser/dom/html_element_factory.h"
#include "cobalt/browser/loader/fake_resource_loader_factory.h"
#include "cobalt/browser/loader/resource_loader_factory.h"
#include "cobalt/browser/script/global_object_proxy.h"
#include "cobalt/browser/script/javascript_engine.h"
#include "cobalt/layout/layout_manager.h"
#include "cobalt/renderer/renderer_module.h"
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
    // TODO(***REMOVED***): Replace with ResourceLoaderFactory once it's implemented.
    FakeResourceLoaderFactory::Options fake_resource_loader_factory_options;
  };

  explicit BrowserModule(const Options& options);
  ~BrowserModule();

  renderer::RendererModule* renderer_module() { return &renderer_module_; }

  DocumentBuilder* document_builder() { return document_builder_.get(); }

  Document* document() { return document_.get(); }

 private:
  // Glue function to deal with the production of a render tree, and will
  // manage handing it off to the renderer.
  void OnRenderTreeProduced(
      const scoped_refptr<render_tree::Node>& render_tree);

  // Sets up everything to do with graphics, from backend objects like the
  // display and graphics context to the rasterizer and rendering pipeline.
  renderer::RendererModule renderer_module_;

  // JavaScript engine for the browser.
  scoped_ptr<script::JavaScriptEngine> javascript_engine_;

  // JavaScript Global Object for the browser. There should be one per window,
  // but since there is only one window, we can have one per browser.
  scoped_refptr<script::GlobalObjectProxy> global_object_proxy_;

  // The loader factory that creates loader that starts the actuall loading.
  scoped_ptr<ResourceLoaderFactory> resource_loader_factory_;

  // Creates HTML elements by name.
  HTMLElementFactory html_element_factory_;

  // The builder that builds the document in the run loop.
  scoped_ptr<DocumentBuilder> document_builder_;

  // The document.
  scoped_refptr<Document> document_;

  // Triggers layout whenever the document changes.
  layout::LayoutManager layout_manager_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // BROWSER_BROWSER_MODULE_H_

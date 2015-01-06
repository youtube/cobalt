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

#include "cobalt/browser/browser_module.h"

#include "base/bind.h"
#include "base/logging.h"

using cobalt::renderer::RendererModule;

namespace cobalt {
namespace browser {

BrowserModule::BrowserModule(const Options& options)
    : renderer_module_(options.renderer_module_options),
      dummy_render_tree_source_(
          renderer_module_.pipeline()->GetResourceProvider(),
          base::Bind(&BrowserModule::OnRenderTreeProduced,
                     base::Unretained(this))),
      resource_loader_factory_(FakeResourceLoaderFactory::Create(
          options.fake_resource_loader_factory_options)),
      document_builder_(
          DocumentBuilder::Create(resource_loader_factory_.get())),
      document_(Document::Create(options.url)) {
  // Start building the document asynchronously.
  document_builder_->BuildDocument(options.url, document_.get());
}

BrowserModule::~BrowserModule() {}

void BrowserModule::OnRenderTreeProduced(
    const scoped_refptr<render_tree::Node>& render_tree) {
  renderer_module_.pipeline()->Submit(render_tree);
}

}  // namespace browser
}  // namespace cobalt

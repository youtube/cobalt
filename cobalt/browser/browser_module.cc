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

namespace cobalt {
namespace browser {
namespace {

// TODO(***REMOVED***): Request viewport size from graphics pipeline and subscribe to
// viewport size changes.
const int kInitialWidth = 1920;
const int kInitialHeight = 1080;

}  // namespace

BrowserModule::BrowserModule(const std::string& user_agent,
                             const Options& options)
    : renderer_module_(options.renderer_module_options),
      css_parser_(css_parser::Parser::Create()),
      javascript_engine_(script::JavaScriptEngine::CreateEngine()),
      global_object_proxy_(javascript_engine_->CreateGlobalObject()),
      script_runner_(
          script::ScriptRunner::CreateScriptRunner(global_object_proxy_)),
      fetcher_factory_(new loader::FetcherFactory()),
      window_(new dom::Window(kInitialWidth, kInitialHeight, css_parser_.get(),
                              fetcher_factory_.get(), script_runner_.get(),
                              options.url, user_agent)),
      layout_manager_(window_.get(),
                      renderer_module_.pipeline()->GetResourceProvider(),
                      base::Bind(&BrowserModule::OnRenderTreeProduced,
                                 base::Unretained(this))),
      input_device_adapter_(window_.get()) {
  // TODO(***REMOVED***): Temporarily bind the document here for Cobalt Oxide.
  global_object_proxy_->Bind("document", window_->document());
}

BrowserModule::~BrowserModule() {}

void BrowserModule::OnRenderTreeProduced(
    const scoped_refptr<render_tree::Node>& render_tree) {
  renderer_module_.pipeline()->Submit(render_tree);
}

}  // namespace browser
}  // namespace cobalt

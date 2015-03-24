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

#include "cobalt/browser/web_module.h"

#include "base/logging.h"

namespace cobalt {
namespace browser {

WebModule::WebModule(const layout::LayoutManager::OnRenderTreeProducedCallback&
                         on_render_tree_produced,
                     const math::Size& window_dimensions,
                     const std::string& user_agent,
                     render_tree::ResourceProvider* resource_provider,
                     const Options& options)
    : css_parser_(css_parser::Parser::Create()),
      javascript_engine_(script::JavaScriptEngine::CreateEngine()),
      global_object_proxy_(javascript_engine_->CreateGlobalObject()),
      script_runner_(
          script::ScriptRunner::CreateScriptRunner(global_object_proxy_)),
      fetcher_factory_(new loader::FetcherFactory()),
      window_(new dom::Window(window_dimensions.width(),
                              window_dimensions.height(), css_parser_.get(),
                              fetcher_factory_.get(), script_runner_.get(),
                              options.url, user_agent)),
      layout_manager_(window_.get(), resource_provider, on_render_tree_produced,
                      options.layout_trigger) {
  global_object_proxy_->SetGlobalInterface(window_);
}

WebModule::~WebModule() {}

void WebModule::InjectEvent(const scoped_refptr<dom::Event>& event) {
  // Forward the input event on to the DOM document.
  thread_checker_.CalledOnValidThread();
  window_->document()->DispatchEvent(event);
}

}  // namespace browser
}  // namespace cobalt

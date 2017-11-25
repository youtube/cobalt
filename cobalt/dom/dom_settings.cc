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

#include "cobalt/dom/dom_settings.h"

#include "cobalt/dom/document.h"
#include "cobalt/dom/url_utils.h"
#include "cobalt/dom/window.h"

namespace cobalt {
namespace dom {

DOMSettings::DOMSettings(
    const int max_dom_element_depth, loader::FetcherFactory* fetcher_factory,
    network::NetworkModule* network_module, const scoped_refptr<Window>& window,
    MediaSourceRegistry* media_source_registry, Blob::Registry* blob_registry,
    media::CanPlayTypeHandler* can_play_type_handler,
    script::JavaScriptEngine* engine,
    script::GlobalEnvironment* global_environment,
    MutationObserverTaskManager* mutation_observer_task_manager,
    const Options& options)
    : max_dom_element_depth_(max_dom_element_depth),
      microphone_options_(options.microphone_options),
      fetcher_factory_(fetcher_factory),
      network_module_(network_module),
      window_(window),
      array_buffer_allocator_(options.array_buffer_allocator),
      array_buffer_cache_(options.array_buffer_cache),
      media_source_registry_(media_source_registry),
      blob_registry_(blob_registry),
      can_play_type_handler_(can_play_type_handler),
      javascript_engine_(engine),
      global_environment_(global_environment),
      mutation_observer_task_manager_(mutation_observer_task_manager) {
  if (array_buffer_allocator_) {
    DCHECK(options.array_buffer_cache);
  } else {
    DCHECK(!options.array_buffer_cache);
  }
}

DOMSettings::~DOMSettings() {}

void DOMSettings::set_window(const scoped_refptr<Window>& window) {
  window_ = window;
}
scoped_refptr<Window> DOMSettings::window() const { return window_; }

const GURL& DOMSettings::base_url() const {
  return window()->document()->url_as_gurl();
}

loader::Origin DOMSettings::document_origin() const {
  return window()->document()->location()->GetOriginAsObject();
}

}  // namespace dom
}  // namespace cobalt

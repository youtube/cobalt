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

#ifndef COBALT_DOM_DOM_SETTINGS_H_
#define COBALT_DOM_DOM_SETTINGS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/window.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {

namespace loader {
class FetcherFactory;
}
namespace network {
class NetworkModule;
}
namespace script {
class GlobalObjectProxy;
class JavaScriptEngine;
}
namespace dom {

// A package of global state to be passed around to script objects
// that ask for it in their IDL custom attributes.
class DOMSettings : public script::EnvironmentSettings {
 public:
  // Hold optional settings for DOMSettings.
  struct Options {
    Options() : array_buffer_allocator(NULL), array_buffer_cache(NULL) {}

    // ArrayBuffer allocates its memory on the heap by default and ArrayBuffers
    // may occupy a lot of memory.  It is possible to provide an allocator via
    // the following member on some platforms so ArrayBuffer can possibly use
    // memory that is not part of the heap.
    ArrayBuffer::Allocator* array_buffer_allocator;
    // When array_buffer_allocator is provided, we still need to hold certain
    // amount of ArrayBuffer inside main memory.  So we have provide the
    // following cache to manage ArrayBuffer in main memory.
    ArrayBuffer::Cache* array_buffer_cache;
  };

  DOMSettings(const int max_dom_element_depth,
              loader::FetcherFactory* fetcher_factory,
              network::NetworkModule* network_module,
              const scoped_refptr<Window>& window,
              MediaSource::Registry* media_source_registry,
              script::JavaScriptEngine* engine,
              script::GlobalObjectProxy* global_object_proxy,
              const Options& options = Options());
  ~DOMSettings() OVERRIDE;

  int max_dom_element_depth() { return max_dom_element_depth_; }

  void set_window(const scoped_refptr<Window>& window) { window_ = window; }
  scoped_refptr<Window> window() const { return window_; }

  ArrayBuffer::Allocator* array_buffer_allocator() const {
    return array_buffer_allocator_;
  }

  ArrayBuffer::Cache* array_buffer_cache() const { return array_buffer_cache_; }

  void set_fetcher_factory(loader::FetcherFactory* fetcher_factory) {
    fetcher_factory_ = fetcher_factory;
  }
  loader::FetcherFactory* fetcher_factory() const { return fetcher_factory_; }
  void set_network_module(network::NetworkModule* network_module) {
    network_module_ = network_module;
  }
  network::NetworkModule* network_module() const { return network_module_; }
  script::JavaScriptEngine* javascript_engine() const {
    return javascript_engine_;
  }
  script::GlobalObjectProxy* global_object() const {
    return global_object_proxy_;
  }
  MediaSource::Registry* media_source_registry() const {
    return media_source_registry_;
  }

  // An absolute URL used to resolve relative URLs.
  virtual GURL base_url() const;

 private:
  const int max_dom_element_depth_;
  loader::FetcherFactory* fetcher_factory_;
  network::NetworkModule* network_module_;
  scoped_refptr<Window> window_;
  ArrayBuffer::Allocator* array_buffer_allocator_;
  ArrayBuffer::Cache* array_buffer_cache_;
  MediaSource::Registry* media_source_registry_;
  script::JavaScriptEngine* javascript_engine_;
  script::GlobalObjectProxy* global_object_proxy_;

  DISALLOW_COPY_AND_ASSIGN(DOMSettings);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOM_SETTINGS_H_

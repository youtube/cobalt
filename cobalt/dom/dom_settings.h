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

#ifndef COBALT_DOM_DOM_SETTINGS_H_
#define COBALT_DOM_DOM_SETTINGS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/dom/array_buffer.h"
#include "cobalt/dom/blob.h"
#include "cobalt/dom/mutation_observer_task_manager.h"
#include "cobalt/dom/url_registry.h"
#include "cobalt/dom/url_utils.h"
#include "cobalt/media/can_play_type_handler.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/speech/microphone.h"

namespace cobalt {

namespace loader {
class FetcherFactory;
}
namespace network {
class NetworkModule;
}
namespace script {
class GlobalEnvironment;
class JavaScriptEngine;
}
namespace dom {
class MediaSource;
class Window;

// A package of global state to be passed around to script objects
// that ask for it in their IDL custom attributes.
class DOMSettings : public script::EnvironmentSettings {
 public:
  typedef UrlRegistry<MediaSource> MediaSourceRegistry;
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
    // Microphone options.
    speech::Microphone::Options microphone_options;
  };

  DOMSettings(const int max_dom_element_depth,
              loader::FetcherFactory* fetcher_factory,
              network::NetworkModule* network_module,
              const scoped_refptr<Window>& window,
              MediaSourceRegistry* media_source_registry,
              Blob::Registry* blob_registry,
              media::CanPlayTypeHandler* can_play_type_handler,
              script::JavaScriptEngine* engine,
              script::GlobalEnvironment* global_environment_proxy,
              MutationObserverTaskManager* mutation_observer_task_manager,
              const Options& options = Options());
  ~DOMSettings() override;

  int max_dom_element_depth() { return max_dom_element_depth_; }
  const speech::Microphone::Options& microphone_options() const {
    return microphone_options_;
  }

  void set_window(const scoped_refptr<Window>& window);
  scoped_refptr<Window> window() const;

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
  script::GlobalEnvironment* global_environment() const {
    return global_environment_;
  }
  MediaSourceRegistry* media_source_registry() const {
    return media_source_registry_;
  }
  media::CanPlayTypeHandler* can_play_type_handler() const {
    return can_play_type_handler_;
  }
  MutationObserverTaskManager* mutation_observer_task_manager() const {
    return mutation_observer_task_manager_;
  }
  Blob::Registry* blob_registry() const { return blob_registry_; }

  // An absolute URL used to resolve relative URLs.
  virtual const GURL& base_url() const;

  // Return's document's origin.
  loader::Origin document_origin() const;

 private:
  const int max_dom_element_depth_;
  const speech::Microphone::Options microphone_options_;
  loader::FetcherFactory* fetcher_factory_;
  network::NetworkModule* network_module_;
  scoped_refptr<Window> window_;
  ArrayBuffer::Allocator* array_buffer_allocator_;
  ArrayBuffer::Cache* array_buffer_cache_;
  MediaSourceRegistry* media_source_registry_;
  Blob::Registry* blob_registry_;
  media::CanPlayTypeHandler* can_play_type_handler_;
  script::JavaScriptEngine* javascript_engine_;
  script::GlobalEnvironment* global_environment_;
  MutationObserverTaskManager* mutation_observer_task_manager_;

  DISALLOW_COPY_AND_ASSIGN(DOMSettings);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOM_SETTINGS_H_

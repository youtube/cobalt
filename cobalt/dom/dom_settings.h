// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cobalt/base/debugger_hooks.h"
#include "cobalt/dom/mutation_observer_task_manager.h"
#include "cobalt/media/can_play_type_handler.h"
#include "cobalt/media/decoder_buffer_memory_info.h"
#include "cobalt/speech/microphone.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/url_registry.h"
#include "cobalt/web/url_utils.h"

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
}  // namespace script
namespace dom {
class MediaSource;
class Window;

// A package of global state to be passed around to script objects
// that ask for it in their IDL custom attributes.
class DOMSettings : public web::EnvironmentSettings {
 public:
  typedef web::UrlRegistry<MediaSource> MediaSourceRegistry;
  // Hold optional settings for DOMSettings.
  struct Options {
    // Microphone options.
    speech::Microphone::Options microphone_options;
  };

  DOMSettings(const base::DebuggerHooks& debugger_hooks,
              const int max_dom_element_depth,
              MediaSourceRegistry* media_source_registry,
              media::CanPlayTypeHandler* can_play_type_handler,
              const media::DecoderBufferMemoryInfo* decoder_buffer_memory_info,
              MutationObserverTaskManager* mutation_observer_task_manager,
              const Options& options = Options());
  ~DOMSettings() override;

  int max_dom_element_depth() { return max_dom_element_depth_; }
  const speech::Microphone::Options& microphone_options() const {
    return microphone_options_;
  }

  Window* window() const;

  MediaSourceRegistry* media_source_registry() const {
    return media_source_registry_;
  }
  void set_decoder_buffer_memory_info(
      const media::DecoderBufferMemoryInfo* decoder_buffer_memory_info) {
    decoder_buffer_memory_info_ = decoder_buffer_memory_info;
  }
  media::CanPlayTypeHandler* can_play_type_handler() const {
    return can_play_type_handler_;
  }
  const media::DecoderBufferMemoryInfo* decoder_buffer_memory_info() {
    return decoder_buffer_memory_info_;
  }
  MutationObserverTaskManager* mutation_observer_task_manager() const {
    return mutation_observer_task_manager_;
  }

  // From: script::EnvironmentSettings
  //
  const GURL& base_url() const override;

  // Return the origin of window's associated Document.
  //   https://html.spec.whatwg.org/#set-up-a-window-environment-settings-object
  loader::Origin GetOrigin() const override;

 private:
  const int max_dom_element_depth_;
  const speech::Microphone::Options microphone_options_;
  MediaSourceRegistry* media_source_registry_;
  media::CanPlayTypeHandler* can_play_type_handler_;
  const media::DecoderBufferMemoryInfo* decoder_buffer_memory_info_;
  MutationObserverTaskManager* mutation_observer_task_manager_;
  DISALLOW_COPY_AND_ASSIGN(DOMSettings);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOM_SETTINGS_H_

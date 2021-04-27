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

#ifndef COBALT_DOM_HTML_ELEMENT_CONTEXT_H_
#define COBALT_DOM_HTML_ELEMENT_CONTEXT_H_

#include <memory>
#include <string>

#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cobalt/base/application_state.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/dom/application_lifecycle_state.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/parser.h"
#include "cobalt/dom/url_registry.h"
#include "cobalt/dom/performance.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/font/remote_typeface_cache.h"
#include "cobalt/loader/image/animated_image_tracker.h"
#include "cobalt/loader/image/image_cache.h"
#include "cobalt/loader/mesh/mesh_cache.h"
#include "cobalt/media/can_play_type_handler.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/script_runner.h"
#include "cobalt/script/script_value_factory.h"

namespace cobalt {
namespace dom {

class HTMLElementFactory;
class MediaSource;

// This class contains references to several objects that are required by HTML
// elements, including HTML element factory, which is used to create new
// HTML elements.
class HTMLElementContext {
 public:
  typedef UrlRegistry<MediaSource> MediaSourceRegistry;

#if !defined(COBALT_BUILD_TYPE_GOLD)
  // No-args constructor for tests.
  HTMLElementContext();
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

  HTMLElementContext(
      script::EnvironmentSettings* environment_settings,
      loader::FetcherFactory* fetcher_factory,
      loader::LoaderFactory* loader_factory, cssom::CSSParser* css_parser,
      Parser* dom_parser, media::CanPlayTypeHandler* can_play_type_handler,
      media::WebMediaPlayerFactory* web_media_player_factory,
      script::ScriptRunner* script_runner,
      script::ScriptValueFactory* script_value_factory,
      MediaSourceRegistry* media_source_registry,
      render_tree::ResourceProvider** resource_provider,
      loader::image::AnimatedImageTracker* animated_image_tracker,
      loader::image::ImageCache* image_cache,
      loader::image::ReducedCacheCapacityManager*
          reduced_image_cache_capacity_manager,
      loader::font::RemoteTypefaceCache* remote_typeface_cache,
      loader::mesh::MeshCache* mesh_cache, DomStatTracker* dom_stat_tracker,
      const std::string& font_language_script,
      base::ApplicationState initial_application_state,
      base::WaitableEvent* synchronous_loader_interrupt,
      Performance* performance,
      bool enable_inline_script_warnings = false,
      float video_playback_rate_multiplier = 1.0);
  ~HTMLElementContext();

  script::EnvironmentSettings* environment_settings() const {
    return environment_settings_;
  }

  loader::FetcherFactory* fetcher_factory() { return fetcher_factory_; }

  loader::LoaderFactory* loader_factory() { return loader_factory_; }

  cssom::CSSParser* css_parser() { return css_parser_; }

  Parser* dom_parser() { return dom_parser_; }

  media::CanPlayTypeHandler* can_play_type_handler() {
    return can_play_type_handler_;
  }
  media::WebMediaPlayerFactory* web_media_player_factory() {
    return web_media_player_factory_;
  }
  void set_web_media_player_factory(
      media::WebMediaPlayerFactory* web_media_player_factory) {
    web_media_player_factory_ = web_media_player_factory;
  }

  script::ScriptRunner* script_runner() const { return script_runner_; }

  script::ScriptValueFactory* script_value_factory() const {
    return script_value_factory_;
  }

  MediaSourceRegistry* media_source_registry() {
    return media_source_registry_;
  }

  render_tree::ResourceProvider** resource_provider() const {
    return resource_provider_;
  }

  base::WaitableEvent* synchronous_loader_interrupt() {
    return synchronous_loader_interrupt_;
  }

  bool enable_inline_script_warnings() const {
    return enable_inline_script_warnings_;
  }

  loader::image::AnimatedImageTracker* animated_image_tracker() const {
    return animated_image_tracker_;
  }

  loader::image::ImageCache* image_cache() const { return image_cache_; }

  loader::font::RemoteTypefaceCache* remote_typeface_cache() const {
    return remote_typeface_cache_;
  }

  loader::mesh::MeshCache* mesh_cache() const { return mesh_cache_; }

  DomStatTracker* dom_stat_tracker() { return dom_stat_tracker_; }

  const std::string& font_language_script() const {
    return font_language_script_;
  }

  float video_playback_rate_multiplier() const {
    return video_playback_rate_multiplier_;
  }

  base::Thread* sync_load_thread() { return &sync_load_thread_; }

  HTMLElementFactory* html_element_factory() {
    return html_element_factory_.get();
  }

  loader::image::ReducedCacheCapacityManager*
  reduced_image_cache_capacity_manager() {
    return reduced_image_cache_capacity_manager_;
  }

  base::WeakPtr<ApplicationLifecycleState> application_lifecycle_state() {
    return application_lifecycle_state_weak_ptr_factory_.GetWeakPtr();
  }

  Performance* performance() { return performance_; }

 private:
#if !defined(COBALT_BUILD_TYPE_GOLD)
  // StubEnvironmentSettings for no-args test constructor.
  std::unique_ptr<script::EnvironmentSettings> stub_environment_settings_;
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

  script::EnvironmentSettings* environment_settings_;
  loader::FetcherFactory* const fetcher_factory_;
  loader::LoaderFactory* const loader_factory_;
  cssom::CSSParser* const css_parser_;
  Parser* const dom_parser_;
  media::CanPlayTypeHandler* can_play_type_handler_;
  media::WebMediaPlayerFactory* web_media_player_factory_;
  script::ScriptRunner* const script_runner_;
  script::ScriptValueFactory* const script_value_factory_;
  MediaSourceRegistry* const media_source_registry_;
  render_tree::ResourceProvider** resource_provider_;
  loader::image::AnimatedImageTracker* const animated_image_tracker_;
  loader::image::ImageCache* const image_cache_;
  loader::image::ReducedCacheCapacityManager* const
      reduced_image_cache_capacity_manager_;
  loader::font::RemoteTypefaceCache* const remote_typeface_cache_;
  loader::mesh::MeshCache* const mesh_cache_;
  DomStatTracker* const dom_stat_tracker_;
  const std::string font_language_script_;
  ApplicationLifecycleState application_lifecycle_state_;
  base::WeakPtrFactory<ApplicationLifecycleState>
      application_lifecycle_state_weak_ptr_factory_;
  const float video_playback_rate_multiplier_;
  base::WaitableEvent* synchronous_loader_interrupt_ = nullptr;
  bool enable_inline_script_warnings_;

  base::Thread sync_load_thread_;
  std::unique_ptr<HTMLElementFactory> html_element_factory_;

  Performance* performance_;

  DISALLOW_COPY_AND_ASSIGN(HTMLElementContext);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_ELEMENT_CONTEXT_H_

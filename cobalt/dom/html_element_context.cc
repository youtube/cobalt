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

#include "cobalt/dom/html_element_context.h"

#include "cobalt/dom/html_element_factory.h"
#include "cobalt/dom/window.h"

#if !defined(COBALT_BUILD_TYPE_GOLD)
#include "cobalt/dom/testing/stub_environment_settings.h"
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

namespace cobalt {
namespace dom {

#if !defined(COBALT_BUILD_TYPE_GOLD)
HTMLElementContext::HTMLElementContext()
    : stub_environment_settings_(new testing::StubEnvironmentSettings),
      environment_settings_(stub_environment_settings_.get()),
      fetcher_factory_(NULL),
      loader_factory_(NULL),
      css_parser_(NULL),
      dom_parser_(NULL),
      can_play_type_handler_(NULL),
      web_media_player_factory_(NULL),
      script_runner_(NULL),
      script_value_factory_(NULL),
      media_source_registry_(NULL),
      resource_provider_(NULL),
      animated_image_tracker_(NULL),
      image_cache_(NULL),
      reduced_image_cache_capacity_manager_(NULL),
      remote_typeface_cache_(NULL),
      mesh_cache_(NULL),
      dom_stat_tracker_(NULL),
      application_lifecycle_state_weak_ptr_factory_(
          &application_lifecycle_state_),
      video_playback_rate_multiplier_(1.f),
      sync_load_thread_("SynchronousLoad"),
      html_element_factory_(new HTMLElementFactory()),
      performance_(NULL) {
  sync_load_thread_.Start();
}
#endif  // !defined(COBALT_BUILD_TYPE_GOLD)

HTMLElementContext::HTMLElementContext(
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
    bool enable_inline_script_warnings, float video_playback_rate_multiplier)
    : environment_settings_(environment_settings),
      fetcher_factory_(fetcher_factory),
      loader_factory_(loader_factory),
      css_parser_(css_parser),
      dom_parser_(dom_parser),
      can_play_type_handler_(can_play_type_handler),
      web_media_player_factory_(web_media_player_factory),
      script_runner_(script_runner),
      script_value_factory_(script_value_factory),
      media_source_registry_(media_source_registry),
      resource_provider_(resource_provider),
      animated_image_tracker_(animated_image_tracker),
      image_cache_(image_cache),
      reduced_image_cache_capacity_manager_(
          reduced_image_cache_capacity_manager),
      remote_typeface_cache_(remote_typeface_cache),
      mesh_cache_(mesh_cache),
      dom_stat_tracker_(dom_stat_tracker),
      font_language_script_(font_language_script),
      application_lifecycle_state_(initial_application_state),
      application_lifecycle_state_weak_ptr_factory_(
          &application_lifecycle_state_),
      video_playback_rate_multiplier_(video_playback_rate_multiplier),
      synchronous_loader_interrupt_(synchronous_loader_interrupt),
      enable_inline_script_warnings_(enable_inline_script_warnings),
      sync_load_thread_("SynchronousLoad"),
      html_element_factory_(new HTMLElementFactory()),
      performance_(performance) {
  sync_load_thread_.Start();
}

HTMLElementContext::~HTMLElementContext() {}

}  // namespace dom
}  // namespace cobalt

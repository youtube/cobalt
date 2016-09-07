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

#include "cobalt/dom/html_element_context.h"

#include "cobalt/dom/html_element_factory.h"

namespace cobalt {
namespace dom {

HTMLElementContext::HTMLElementContext(
    loader::FetcherFactory* fetcher_factory, cssom::CSSParser* css_parser,
    Parser* dom_parser, media::CanPlayTypeHandler* can_play_type_handler,
    media::WebMediaPlayerFactory* web_media_player_factory,
    script::ScriptRunner* script_runner,
    MediaSource::Registry* media_source_registry,
    render_tree::ResourceProvider* resource_provider,
    loader::image::ImageCache* image_cache,
    loader::font::RemoteTypefaceCache* remote_typeface_cache,
    DomStatTracker* dom_stat_tracker, const std::string& language)
    : fetcher_factory_(fetcher_factory),
      css_parser_(css_parser),
      dom_parser_(dom_parser),
      can_play_type_handler_(can_play_type_handler),
      web_media_player_factory_(web_media_player_factory),
      script_runner_(script_runner),
      media_source_registry_(media_source_registry),
      resource_provider_(resource_provider),
      image_cache_(image_cache),
      remote_typeface_cache_(remote_typeface_cache),
      dom_stat_tracker_(dom_stat_tracker),
      language_(language),
      sync_load_thread_("Synchronous Load"),
      html_element_factory_(new HTMLElementFactory()) {
  sync_load_thread_.Start();
}

HTMLElementContext::~HTMLElementContext() {}

}  // namespace dom
}  // namespace cobalt

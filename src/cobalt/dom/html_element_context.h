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

#ifndef COBALT_DOM_HTML_ELEMENT_CONTEXT_H_
#define COBALT_DOM_HTML_ELEMENT_CONTEXT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/dom/dom_stat_tracker.h"
#include "cobalt/dom/media_source.h"
#include "cobalt/dom/parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/font/remote_typeface_cache.h"
#include "cobalt/loader/image/image_cache.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/script/script_runner.h"

namespace cobalt {
namespace dom {

class HTMLElementFactory;

// This class contains references to several objects that are required by HTML
// elements, including HTML element factory, which is used to create new
// HTML elements.
class HTMLElementContext {
 public:
  HTMLElementContext(loader::FetcherFactory* fetcher_factory,
                     cssom::CSSParser* css_parser, Parser* dom_parser,
                     media::WebMediaPlayerFactory* web_media_player_factory,
                     script::ScriptRunner* script_runner,
                     MediaSource::Registry* media_source_registry,
                     render_tree::ResourceProvider* resource_provider,
                     loader::image::ImageCache* image_cache,
                     loader::font::RemoteTypefaceCache* remote_typeface_cache,
                     DomStatTracker* dom_stat_tracker,
                     const std::string& language);
  ~HTMLElementContext();

  loader::FetcherFactory* fetcher_factory() { return fetcher_factory_; }

  cssom::CSSParser* css_parser() { return css_parser_; }

  Parser* dom_parser() { return dom_parser_; }

  media::WebMediaPlayerFactory* web_media_player_factory() {
    return web_media_player_factory_;
  }

  script::ScriptRunner* script_runner() { return script_runner_; }
  MediaSource::Registry* media_source_registry() {
    return media_source_registry_;
  }

  render_tree::ResourceProvider* resource_provider() const {
    return resource_provider_;
  }

  loader::image::ImageCache* image_cache() const { return image_cache_; }

  loader::font::RemoteTypefaceCache* remote_typeface_cache() const {
    return remote_typeface_cache_;
  }

  DomStatTracker* dom_stat_tracker() { return dom_stat_tracker_; }

  const std::string& language() const { return language_; }

  base::Thread* sync_load_thread() { return &sync_load_thread_; }

  HTMLElementFactory* html_element_factory() {
    return html_element_factory_.get();
  }

 private:
  loader::FetcherFactory* const fetcher_factory_;
  cssom::CSSParser* const css_parser_;
  Parser* const dom_parser_;
  media::WebMediaPlayerFactory* const web_media_player_factory_;
  script::ScriptRunner* const script_runner_;
  MediaSource::Registry* const media_source_registry_;
  render_tree::ResourceProvider* resource_provider_;
  loader::image::ImageCache* const image_cache_;
  loader::font::RemoteTypefaceCache* const remote_typeface_cache_;
  DomStatTracker* const dom_stat_tracker_;
  const std::string language_;

  base::Thread sync_load_thread_;
  scoped_ptr<HTMLElementFactory> html_element_factory_;

  DISALLOW_COPY_AND_ASSIGN(HTMLElementContext);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_ELEMENT_CONTEXT_H_

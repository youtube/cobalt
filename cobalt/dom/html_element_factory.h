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

#ifndef DOM_HTML_ELEMENT_FACTORY_H_
#define DOM_HTML_ELEMENT_FACTORY_H_

#include "cobalt/dom/html_element.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/string_piece.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/script/script_runner.h"

namespace cobalt {
namespace dom {

// This factory is responsible for creating HTML elements according to tag name.
class HTMLElementFactory {
 public:
  HTMLElementFactory(loader::FetcherFactory* fetcher_factory,
                     cssom::CSSParser* css_parser,
                     media::WebMediaPlayerFactory* web_media_player_factory,
                     script::ScriptRunner* script_runner);
  ~HTMLElementFactory();

  scoped_refptr<HTMLElement> CreateHTMLElement(
      const base::StringPiece& tag_name);

 private:
  typedef base::Callback<scoped_refptr<HTMLElement>()>
      CreateHTMLElementTCallback;
  typedef base::hash_map<base::StringPiece, CreateHTMLElementTCallback>
      TagNameToCreateHTMLElementTCallbackMap;

  template <typename T>
  scoped_refptr<HTMLElement> CreateHTMLElementT();

  loader::FetcherFactory* const fetcher_factory_;
  cssom::CSSParser* const css_parser_;
  media::WebMediaPlayerFactory* web_media_player_factory_;
  script::ScriptRunner* const script_runner_;

  TagNameToCreateHTMLElementTCallbackMap
      tag_name_to_create_html_element_t_callback_map_;

  DISALLOW_COPY_AND_ASSIGN(HTMLElementFactory);
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_HTML_ELEMENT_FACTORY_H_

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

#include "cobalt/dom/html_element_factory.h"

#include "base/logging.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_div_element.h"
#include "cobalt/dom/html_head_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/html_link_element.h"
#include "cobalt/dom/html_media_element.h"
#include "cobalt/dom/html_script_element.h"
#include "cobalt/dom/html_span_element.h"
#include "cobalt/dom/html_style_element.h"
#include "cobalt/dom/html_unknown_element.h"

namespace cobalt {
namespace dom {

template <typename T>
scoped_refptr<HTMLElement> HTMLElementFactory::CreateHTMLElementT() {
  return new T(this, css_parser_);
}

template <>
scoped_refptr<HTMLElement>
HTMLElementFactory::CreateHTMLElementT<HTMLLinkElement>() {
  return new HTMLLinkElement(this, css_parser_, fetcher_factory_);
}

template <>
scoped_refptr<HTMLElement>
HTMLElementFactory::CreateHTMLElementT<HTMLScriptElement>() {
  return new HTMLScriptElement(this, css_parser_, fetcher_factory_,
                               script_runner_);
}

template <>
scoped_refptr<HTMLElement>
HTMLElementFactory::CreateHTMLElementT<HTMLMediaElement>() {
  return new HTMLMediaElement(this, css_parser_, fetcher_factory_,
                              web_media_player_factory_);
}

HTMLElementFactory::HTMLElementFactory(
    loader::FetcherFactory* fetcher_factory, cssom::CSSParser* css_parser,
    media::WebMediaPlayerFactory* web_media_player_factory,
    script::ScriptRunner* script_runner)
    : fetcher_factory_(fetcher_factory),
      css_parser_(css_parser),
      web_media_player_factory_(web_media_player_factory),
      script_runner_(script_runner) {
  tag_name_to_create_html_element_t_callback_map_[HTMLBodyElement::kTagName] =
      base::Bind(&HTMLElementFactory::CreateHTMLElementT<HTMLBodyElement>,
                 base::Unretained(this));
  tag_name_to_create_html_element_t_callback_map_[HTMLDivElement::kTagName] =
      base::Bind(&HTMLElementFactory::CreateHTMLElementT<HTMLDivElement>,
                 base::Unretained(this));
  tag_name_to_create_html_element_t_callback_map_[HTMLHeadElement::kTagName] =
      base::Bind(&HTMLElementFactory::CreateHTMLElementT<HTMLHeadElement>,
                 base::Unretained(this));
  tag_name_to_create_html_element_t_callback_map_[HTMLHtmlElement::kTagName] =
      base::Bind(&HTMLElementFactory::CreateHTMLElementT<HTMLHtmlElement>,
                 base::Unretained(this));
  tag_name_to_create_html_element_t_callback_map_[HTMLLinkElement::kTagName] =
      base::Bind(&HTMLElementFactory::CreateHTMLElementT<HTMLLinkElement>,
                 base::Unretained(this));
  tag_name_to_create_html_element_t_callback_map_[HTMLMediaElement::kTagName] =
      base::Bind(&HTMLElementFactory::CreateHTMLElementT<HTMLMediaElement>,
                 base::Unretained(this));
  tag_name_to_create_html_element_t_callback_map_[HTMLScriptElement::kTagName] =
      base::Bind(&HTMLElementFactory::CreateHTMLElementT<HTMLScriptElement>,
                 base::Unretained(this));
  tag_name_to_create_html_element_t_callback_map_[HTMLSpanElement::kTagName] =
      base::Bind(&HTMLElementFactory::CreateHTMLElementT<HTMLSpanElement>,
                 base::Unretained(this));
  tag_name_to_create_html_element_t_callback_map_[HTMLStyleElement::kTagName] =
      base::Bind(&HTMLElementFactory::CreateHTMLElementT<HTMLStyleElement>,
                 base::Unretained(this));
}

HTMLElementFactory::~HTMLElementFactory() {}

scoped_refptr<HTMLElement> HTMLElementFactory::CreateHTMLElement(
    const base::StringPiece& tag_name) {
  TagNameToCreateHTMLElementTCallbackMap::const_iterator iter =
      tag_name_to_create_html_element_t_callback_map_.find(tag_name);
  if (iter != tag_name_to_create_html_element_t_callback_map_.end()) {
    return iter->second.Run();
  } else {
    // TODO(***REMOVED***): Report unknown HTML tag.
    return new HTMLUnknownElement(this, css_parser_, tag_name);
  }
}

}  // namespace dom
}  // namespace cobalt

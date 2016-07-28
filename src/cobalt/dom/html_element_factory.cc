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

#include "base/bind.h"
#include "cobalt/dom/html_anchor_element.h"
#include "cobalt/dom/html_body_element.h"
#include "cobalt/dom/html_br_element.h"
#include "cobalt/dom/html_div_element.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_head_element.h"
#include "cobalt/dom/html_heading_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/html_image_element.h"
#include "cobalt/dom/html_link_element.h"
#include "cobalt/dom/html_meta_element.h"
#include "cobalt/dom/html_paragraph_element.h"
#include "cobalt/dom/html_script_element.h"
#include "cobalt/dom/html_span_element.h"
#include "cobalt/dom/html_style_element.h"
#include "cobalt/dom/html_title_element.h"
#include "cobalt/dom/html_unknown_element.h"
#include "cobalt/dom/html_video_element.h"

namespace cobalt {
namespace dom {
namespace {

// Bind these HTML element creation functions to create callbacks that are
// indexed in the hash_map.

template <typename T>
scoped_refptr<HTMLElement> CreateHTMLElementT(Document* document) {
  return new T(document);
}

template <typename T>
scoped_refptr<HTMLElement> CreateHTMLElementWithTagNameT(
    const std::string& tag_name, Document* document) {
  return new T(document, base::Token(tag_name));
}

}  // namespace

HTMLElementFactory::HTMLElementFactory() {
  // Register HTML elements that have only one tag name in the map.
  RegisterHTMLElementWithSingleTagName<HTMLAnchorElement>();
  RegisterHTMLElementWithSingleTagName<HTMLBodyElement>();
  RegisterHTMLElementWithSingleTagName<HTMLBRElement>();
  RegisterHTMLElementWithSingleTagName<HTMLDivElement>();
  RegisterHTMLElementWithSingleTagName<HTMLHeadElement>();
  RegisterHTMLElementWithSingleTagName<HTMLHtmlElement>();
  RegisterHTMLElementWithSingleTagName<HTMLImageElement>();
  RegisterHTMLElementWithSingleTagName<HTMLLinkElement>();
  RegisterHTMLElementWithSingleTagName<HTMLMetaElement>();
  RegisterHTMLElementWithSingleTagName<HTMLParagraphElement>();
  RegisterHTMLElementWithSingleTagName<HTMLScriptElement>();
  RegisterHTMLElementWithSingleTagName<HTMLSpanElement>();
  RegisterHTMLElementWithSingleTagName<HTMLStyleElement>();
  RegisterHTMLElementWithSingleTagName<HTMLTitleElement>();
  RegisterHTMLElementWithSingleTagName<HTMLVideoElement>();

  // Register HTML elements that have multiple tag names in the map.
  RegisterHTMLElementWithMultipleTagName<HTMLHeadingElement>();
}

HTMLElementFactory::~HTMLElementFactory() {}

scoped_refptr<HTMLElement> HTMLElementFactory::CreateHTMLElement(
    Document* document, base::Token tag_name) {
  TagNameToCreateHTMLElementTCallbackMap::const_iterator iter =
      tag_name_to_create_html_element_t_callback_map_.find(tag_name);
  if (iter != tag_name_to_create_html_element_t_callback_map_.end()) {
    return iter->second.Run(document);
  } else {
    LOG(WARNING) << "Unknown HTML element: <" << tag_name << ">.";
    return new HTMLUnknownElement(document, tag_name);
  }
}

template <typename T>
void HTMLElementFactory::RegisterHTMLElementWithSingleTagName() {
  tag_name_to_create_html_element_t_callback_map_[base::Token(T::kTagName)] =
      base::Bind(&CreateHTMLElementT<T>);
}

template <typename T>
void HTMLElementFactory::RegisterHTMLElementWithMultipleTagName() {
  for (int i = 0; i < T::kTagNameCount; i++) {
    std::string tag_name = T::kTagNames[i];
    tag_name_to_create_html_element_t_callback_map_[base::Token(tag_name)] =
        base::Bind(&CreateHTMLElementWithTagNameT<T>, tag_name);
  }
}

}  // namespace dom
}  // namespace cobalt

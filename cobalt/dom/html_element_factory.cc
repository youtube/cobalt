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
#include "cobalt/dom/htmlbr_element.h"
#include "cobalt/dom/html_div_element.h"
#include "cobalt/dom/html_element.h"
#include "cobalt/dom/html_head_element.h"
#include "cobalt/dom/html_heading_element.h"
#include "cobalt/dom/html_html_element.h"
#include "cobalt/dom/html_link_element.h"
#include "cobalt/dom/html_paragraph_element.h"
#include "cobalt/dom/html_script_element.h"
#include "cobalt/dom/html_span_element.h"
#include "cobalt/dom/html_style_element.h"
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

scoped_refptr<HTMLElement> CreateHTMLHeadingElement(const std::string& tag_name,
                                                    Document* document) {
  return new HTMLHeadingElement(document, tag_name);
}

}  // namespace

HTMLElementFactory::HTMLElementFactory() {
#define ADD_HTML_ELEMENT_CREATION_MAP_ENTRY(name)                   \
  tag_name_to_create_html_element_t_callback_map_[name::kTagName] = \
      base::Bind(&CreateHTMLElementT<name>)
  ADD_HTML_ELEMENT_CREATION_MAP_ENTRY(HTMLAnchorElement);
  ADD_HTML_ELEMENT_CREATION_MAP_ENTRY(HTMLBodyElement);
  ADD_HTML_ELEMENT_CREATION_MAP_ENTRY(HTMLBRElement);
  ADD_HTML_ELEMENT_CREATION_MAP_ENTRY(HTMLDivElement);
  ADD_HTML_ELEMENT_CREATION_MAP_ENTRY(HTMLHeadElement);
  ADD_HTML_ELEMENT_CREATION_MAP_ENTRY(HTMLHtmlElement);
  ADD_HTML_ELEMENT_CREATION_MAP_ENTRY(HTMLLinkElement);
  ADD_HTML_ELEMENT_CREATION_MAP_ENTRY(HTMLParagraphElement);
  ADD_HTML_ELEMENT_CREATION_MAP_ENTRY(HTMLScriptElement);
  ADD_HTML_ELEMENT_CREATION_MAP_ENTRY(HTMLSpanElement);
  ADD_HTML_ELEMENT_CREATION_MAP_ENTRY(HTMLStyleElement);
  ADD_HTML_ELEMENT_CREATION_MAP_ENTRY(HTMLVideoElement);
#undef ADD_HTML_ELEMENT_CREATION_MAP_ENTRY

  for (int i = 0; i < HTMLHeadingElement::kTagNameCount; i++) {
    std::string tag_name = HTMLHeadingElement::kTagNames[i];
    tag_name_to_create_html_element_t_callback_map_[tag_name] =
        base::Bind(&CreateHTMLHeadingElement, tag_name);
  }
}

HTMLElementFactory::~HTMLElementFactory() {}

scoped_refptr<HTMLElement> HTMLElementFactory::CreateHTMLElement(
    Document* document, const std::string& tag_name) {
  TagNameToCreateHTMLElementTCallbackMap::const_iterator iter =
      tag_name_to_create_html_element_t_callback_map_.find(tag_name);
  if (iter != tag_name_to_create_html_element_t_callback_map_.end()) {
    return iter->second.Run(document);
  } else {
    // TODO(***REMOVED***): Report unknown HTML tag.
    return new HTMLUnknownElement(document, tag_name);
  }
}

}  // namespace dom
}  // namespace cobalt

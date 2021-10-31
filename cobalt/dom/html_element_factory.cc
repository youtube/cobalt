// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/html_element_factory.h"

#include "base/bind.h"
#include "base/third_party/icu/icu_utf.h"
#include "cobalt/dom/html_anchor_element.h"
#include "cobalt/dom/html_audio_element.h"
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
#include "cobalt/dom/lottie_player.h"
#include "nb/memory_scope.h"

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
    const std::string& local_name, Document* document) {
  return new T(document, base::Token(local_name));
}

bool IsValidAsciiChar(char32_t c) {
  const bool isLowerAscii = c >= 'a' && c <= 'z';
  const bool isLowerDigit = c >= '0' && c <= '9';
  return isLowerAscii || isLowerDigit || c == '.' || c == '_';
}

// Grandfathered HTML elements that meet CustomElement naming spec:
// https://html.spec.whatwg.org/multipage/custom-elements.html#valid-custom-element-name
bool IsBlacklistedTag(const char* tag, int length) {
  switch (length) {
    case 9:
      if (strcmp(tag, "font-face") == 0) {
        return true;
      }
      break;
    case 13:
      if (strcmp(tag, "font-face-src") == 0 ||
          strcmp(tag, "missing-glyph") == 0 ||
          strcmp(tag, "color-profile") == 0 ||
          strcmp(tag, "font-face-uri") == 0) {
        return true;
      }
      break;
    case 14:
      if (strcmp(tag, "font-face-name") == 0 ||
          strcmp(tag, "annotation-xml") == 0) {
        return true;
      }
      break;
    case 16:
      if (strcmp(tag, "font-face-format") == 0) {
        return true;
      }
      break;
    default:
      return false;
  }
  return false;
}

// Follows naming spec at
// https://html.spec.whatwg.org/multipage/custom-elements.html#valid-custom-element-name
bool IsValidCustomElementName(base::Token tag_name) {
  // Consider adding support for customElements.define in order to
  // formally register CustomElements rather than filter out errors
  // for all potential custom names.

  const char* tag = tag_name.c_str();
  char c = tag[0];
  if (c < 'a' || c > 'z') {
    return false;
  }
  bool contains_hyphen = false;
  int length = 1;

  while ((c = tag[length]) != '\0') {
    // Early return to avoid utf32 conversion cost for most cases.
    if (IsValidAsciiChar(c)) {
      length++;
      continue;
    }
    if (c == '-') {
      contains_hyphen = true;
      length++;
      continue;
    }
    base_icu::UChar32 c32;

    CBU8_NEXT(tag, length, -1, c32);
    bool is_valid_char =
        c32 == 0xb7 || (c32 >= 0xc0 && c32 <= 0xd6) ||
        (c32 >= 0xd8 && c32 <= 0xf6) || (c32 >= 0xf8 && c32 <= 0x037d) ||
        (c32 >= 0x037f && c32 <= 0x1fff) || (c32 >= 0x200c && c32 <= 0x200d) ||
        (c32 >= 0x203f && c32 <= 0x2040) || (c32 >= 0x2070 && c32 <= 0x218f) ||
        (c32 >= 0x2c00 && c32 <= 0x2fef) || (c32 >= 0x3001 && c32 <= 0xd7ff) ||
        (c32 >= 0xf900 && c32 <= 0xfdcf) || (c32 >= 0xfdf0 && c32 <= 0xfffd) ||
        (c32 >= 0x00010000 && c32 <= 0x000effff);
    if (!is_valid_char) {
      return false;
    }
  }

  return contains_hyphen && !IsBlacklistedTag(tag, length);
}

}  // namespace

HTMLElementFactory::HTMLElementFactory() {
  // Register HTML elements that have only one tag name in the map.
  RegisterHTMLElementWithSingleTagName<HTMLAnchorElement>();
  RegisterHTMLElementWithSingleTagName<HTMLAudioElement>();
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
  RegisterHTMLElementWithSingleTagName<LottiePlayer>();

  // Register HTML elements that have multiple tag names in the map.
  RegisterHTMLElementWithMultipleTagName<HTMLHeadingElement>();
}

HTMLElementFactory::~HTMLElementFactory() {}

scoped_refptr<HTMLElement> HTMLElementFactory::CreateHTMLElement(
    Document* document, base::Token tag_name) {
  TRACK_MEMORY_SCOPE("DOM");
  TagNameToCreateHTMLElementTCallbackMap::const_iterator iter =
      tag_name_to_create_html_element_t_callback_map_.find(tag_name);
  if (iter != tag_name_to_create_html_element_t_callback_map_.end()) {
    return iter->second.Run(document);
  } else {
    LOG_IF(WARNING, !IsValidCustomElementName(tag_name))
        << "Unknown HTML element: <" << tag_name << ">.";
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

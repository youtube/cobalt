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

#ifndef DOM_HTML_BODY_ELEMENT_H_
#define DOM_HTML_BODY_ELEMENT_H_

#include "cobalt/dom/html_element.h"

namespace cobalt {
namespace dom {

class Document;

// The body element represents the content of the document.
//   http://www.w3.org/TR/html5/sections.html#the-body-element
class HTMLBodyElement : public HTMLElement {
 public:
  static const char* kTagName;

  HTMLBodyElement(HTMLElementFactory* html_element_factory,
                  cssom::CSSParser* css_parser);

  // Web API: Element
  const std::string& tag_name() const OVERRIDE;

  // Custom, not in any spec.
  scoped_refptr<HTMLBodyElement> AsHTMLBodyElement() OVERRIDE { return this; }

  DEFINE_WRAPPABLE_TYPE(HTMLBodyElement);

 private:
  ~HTMLBodyElement() OVERRIDE;

  void AttachToDocument(Document* document) OVERRIDE;
  void DetachFromDocument() OVERRIDE;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_HTML_BODY_ELEMENT_H_

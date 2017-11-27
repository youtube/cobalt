// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_HTML_DIV_ELEMENT_H_
#define COBALT_DOM_HTML_DIV_ELEMENT_H_

#include <string>

#include "cobalt/dom/html_element.h"

namespace cobalt {
namespace dom {

class Document;

// The div element has no special meaning at all. It represents its children. It
// can be used with the class, lang, and title attributes to mark up semantics
// common to a group of consecutive elements.
//   https://www.w3.org/TR/html5/grouping-content.html#the-div-element
class HTMLDivElement : public HTMLElement {
 public:
  static const char kTagName[];

  explicit HTMLDivElement(Document* document)
      : HTMLElement(document, base::Token(kTagName)) {}

  // Custom, not in any spec.
  scoped_refptr<HTMLDivElement> AsHTMLDivElement() override { return this; }

  DEFINE_WRAPPABLE_TYPE(HTMLDivElement);

 private:
  ~HTMLDivElement() override {}
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_DIV_ELEMENT_H_

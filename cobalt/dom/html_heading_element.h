// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_HTML_HEADING_ELEMENT_H_
#define COBALT_DOM_HTML_HEADING_ELEMENT_H_

#include <string>

#include "cobalt/dom/html_element.h"

namespace cobalt {
namespace dom {

class Document;

// These elements represent headings for their sections.
//   https://www.w3.org/TR/html5/sections.html#the-h1,-h2,-h3,-h4,-h5,-and-h6-elements
class HTMLHeadingElement : public HTMLElement {
 public:
  static const int kTagNameCount;
  static const char* kTagNames[];

  HTMLHeadingElement(Document* document, base::Token tag_name)
      : HTMLElement(document, tag_name) {}

  // Custom, not in any spec.
  scoped_refptr<HTMLHeadingElement> AsHTMLHeadingElement() override {
    return this;
  }

  DEFINE_WRAPPABLE_TYPE(HTMLHeadingElement);

 private:
  ~HTMLHeadingElement() override {}
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_HEADING_ELEMENT_H_

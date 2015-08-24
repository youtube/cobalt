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

#ifndef DOM_HTML_STYLE_ELEMENT_H_
#define DOM_HTML_STYLE_ELEMENT_H_

#include <string>

#include "cobalt/base/source_location.h"
#include "cobalt/dom/html_element.h"

namespace cobalt {
namespace dom {

class Document;

// The style element allows authors to embed style information in their
// documents.
//   http://www.w3.org/TR/html5/document-metadata.html#the-style-element
class HTMLStyleElement : public HTMLElement {
 public:
  static const char* kTagName;

  explicit HTMLStyleElement(Document* document)
      : HTMLElement(document),
        content_location_("[object HTMLStyleElement]", 1, 1) {}

  // Web API: Element
  std::string tag_name() const OVERRIDE;

  // Web API: HTMLStyleElement
  std::string type() const { return GetAttribute("type").value_or(""); }
  void set_type(const std::string& value) { SetAttribute("type", value); }

  // Custom, not in any spec.
  //
  // From HTMLElement.
  scoped_refptr<HTMLStyleElement> AsHTMLStyleElement() OVERRIDE { return this; }

  void SetOpeningTagLocation(
      const base::SourceLocation& opening_tag_location) OVERRIDE;

  // From Node.
  void OnInsertedIntoDocument() OVERRIDE;

  DEFINE_WRAPPABLE_TYPE(HTMLStyleElement);

 private:
  ~HTMLStyleElement() OVERRIDE {}

  base::SourceLocation content_location_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_HTML_STYLE_ELEMENT_H_

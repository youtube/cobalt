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

#ifndef COBALT_DOM_HTML_META_ELEMENT_H_
#define COBALT_DOM_HTML_META_ELEMENT_H_

#include <string>

#include "cobalt/dom/html_element.h"

namespace cobalt {
namespace dom {

class Document;

// The meta element allows authors to meta their document to other resources.
//   https://www.w3.org/TR/html5/document-metadata.html#the-meta-element
class HTMLMetaElement : public HTMLElement {
 public:
  static const char kTagName[];

  explicit HTMLMetaElement(Document* document)
      : HTMLElement(document, base::Token(kTagName)) {}

  // Web API: HTMLMetaElement
  //
  std::string name() const { return GetAttribute("name").value_or(""); }
  void set_name(const std::string& value) { SetAttribute("name", value); }

  std::string http_equiv() const {
    return GetAttribute("http-equiv").value_or("");
  }
  void set_http_equiv(const std::string& value) {
    SetAttribute("http-equiv", value);
  }

  std::string content() const { return GetAttribute("content").value_or(""); }
  void set_content(const std::string& value) { SetAttribute("content", value); }

  // Custom, not in any spec.
  //
  scoped_refptr<HTMLMetaElement> AsHTMLMetaElement() override { return this; }

  // From Node.
  void OnInsertedIntoDocument() override;

  DEFINE_WRAPPABLE_TYPE(HTMLMetaElement);

 private:
  ~HTMLMetaElement() override {}

  bool IsDescendantOfHeadElement() const;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_HTML_META_ELEMENT_H_

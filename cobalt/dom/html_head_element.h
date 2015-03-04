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

#ifndef DOM_HTML_HEAD_ELEMENT_H_
#define DOM_HTML_HEAD_ELEMENT_H_

#include "cobalt/dom/html_element.h"

namespace cobalt {
namespace dom {

class Document;

// The head element represents a collection of metadata for the Document.
//    http://www.w3.org/TR/html/document-metadata.html#the-head-element
class HTMLHeadElement : public HTMLElement {
 public:
  static const char* kTagName;

  static scoped_refptr<HTMLHeadElement> Create();

  // Web API: Element
  const std::string& tag_name() const OVERRIDE;

  // Custom, not in any spec.
  scoped_refptr<HTMLHeadElement> AsHTMLHeadElement() OVERRIDE { return this; }

 private:
  HTMLHeadElement();
  ~HTMLHeadElement() OVERRIDE;

  void AttachToDocument(Document* document) OVERRIDE;
  void DetachFromDocument() OVERRIDE;
};

}  // namespace dom
}  // namespace cobalt

#endif  // DOM_HTML_HEAD_ELEMENT_H_

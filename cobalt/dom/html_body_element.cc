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

#include "cobalt/dom/html_body_element.h"

#include "cobalt/dom/document.h"

namespace cobalt {
namespace dom {

// static
const char* HTMLBodyElement::kTagName = "body";

HTMLBodyElement::HTMLBodyElement(HTMLElementContext* html_element_context)
    : HTMLElement(html_element_context) {}

const std::string& HTMLBodyElement::tag_name() const {
  static const std::string kBodyTagString(kTagName);
  return kBodyTagString;
}

HTMLBodyElement::~HTMLBodyElement() {}

void HTMLBodyElement::AttachToDocument(Document* document) {
  HTMLElement::AttachToDocument(document);
  owner_document()->SetBodyInternal(this);
}

void HTMLBodyElement::DetachFromDocument() {
  owner_document()->SetBodyInternal(NULL);
  Node::DetachFromDocument();
}

}  // namespace dom
}  // namespace cobalt

// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/dom_implementation.h"

#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/element.h"
#include "cobalt/dom/window.h"

namespace cobalt {
namespace dom {

DOMImplementation::DOMImplementation(script::EnvironmentSettings* settings) {
  html_element_context_ = base::polymorphic_downcast<DOMSettings*>(settings)
                              ->window()
                              ->html_element_context();
}

scoped_refptr<XMLDocument> DOMImplementation::CreateDocument(
    base::Optional<std::string> namespace_name,
    const std::string& qualified_name) {
  return CreateDocument(namespace_name, qualified_name, NULL);
}

// Algorithm for CreateDocument:
//   https://www.w3.org/TR/dom/#dom-domimplementation-createdocument
scoped_refptr<XMLDocument> DOMImplementation::CreateDocument(
    base::Optional<std::string> namespace_name,
    const std::string& qualified_name, scoped_refptr<DocumentType> doctype) {

  // 1. Let document be a new XMLDocument.
  scoped_refptr<XMLDocument> document = new XMLDocument(html_element_context_);

  // 2. Let element be null.
  scoped_refptr<Element> element;

  // 3. If qualifiedName is not the empty string, set element to the result of
  // invoking the createElementNS() method with the arguments namespace and
  // qualifiedName on document. Rethrow any exceptions.
  if (!qualified_name.empty()) {
    element =
        document->CreateElementNS(namespace_name.value_or(""), qualified_name);
  }

  // 4. Not needed by Cobalt.

  // 5. If element is not null, append element to document.
  if (element) {
    document->AppendChild(element);
  }

  // 6. Not needed by Cobalt.

  // 7. Return document.
  return document;
}

}  // namespace dom
}  // namespace cobalt

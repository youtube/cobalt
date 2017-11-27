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

#ifndef COBALT_DOM_DOM_IMPLEMENTATION_H_
#define COBALT_DOM_DOM_IMPLEMENTATION_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "cobalt/dom/document_type.h"
#include "cobalt/dom/xml_document.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class DOMSettings;
class HTMLElementContext;

// The DOMImplementation interface represent an object providing methods which
// are not dependent on any particular document. Such an object is returned by
// the Document.implementation property.
//   https://www.w3.org/TR/dom/#domimplementation
class DOMImplementation : public script::Wrappable {
 public:
  explicit DOMImplementation(script::EnvironmentSettings* settings);
  explicit DOMImplementation(HTMLElementContext* html_element_context)
      : html_element_context_(html_element_context) {}
  ~DOMImplementation() override {}

  // Web API: DOMImplementation
  scoped_refptr<XMLDocument> CreateDocument(
      base::optional<std::string> namespace_name,
      const std::string& qualified_name);
  scoped_refptr<XMLDocument> CreateDocument(
      base::optional<std::string> namespace_name,
      const std::string& qualified_name, scoped_refptr<DocumentType> doctype);

  DEFINE_WRAPPABLE_TYPE(DOMImplementation);

 private:
  HTMLElementContext* html_element_context_;

  DISALLOW_COPY_AND_ASSIGN(DOMImplementation);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_DOM_IMPLEMENTATION_H_

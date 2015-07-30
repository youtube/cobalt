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

#include "cobalt/dom/dom_implementation.h"

namespace cobalt {
namespace dom {

scoped_refptr<XMLDocument> DOMImplementation::CreateDocument(
    base::optional<std::string> namespace_name,
    const std::string& qualified_name) {
  return CreateDocument(namespace_name, qualified_name, NULL);
}

scoped_refptr<XMLDocument> DOMImplementation::CreateDocument(
    base::optional<std::string> namespace_name,
    const std::string& qualified_name, scoped_refptr<DocumentType> doctype) {
  UNREFERENCED_PARAMETER(namespace_name);
  UNREFERENCED_PARAMETER(qualified_name);
  DCHECK(!doctype);
  return new XMLDocument();
}

}  // namespace dom
}  // namespace cobalt

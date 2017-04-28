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

#include "cobalt/dom/dom_implementation.h"

#include "base/memory/ref_counted.h"
#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element_context.h"
#include "cobalt/dom/xml_document.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

TEST(DOMImplementationTest, CreateDocumentShouldCreateXMLDocument) {
  HTMLElementContext html_element_context;
  scoped_refptr<DOMImplementation> dom_implementation =
      new DOMImplementation(&html_element_context);
  scoped_refptr<Document> document =
      dom_implementation->CreateDocument(base::optional<std::string>(""), "");
  EXPECT_TRUE(document->IsXMLDocument());
}

}  // namespace dom
}  // namespace cobalt

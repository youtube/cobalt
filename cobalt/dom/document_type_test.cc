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

#include "cobalt/dom/document_type.h"

#include "cobalt/dom/document.h"
#include "cobalt/dom/html_element_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace dom {

class DocumentTypeTest : public ::testing::Test {
 protected:
  DocumentTypeTest() : document_(new Document(&html_element_context_)) {}
  ~DocumentTypeTest() override {}

  HTMLElementContext html_element_context_;
  scoped_refptr<Document> document_;
};

TEST_F(DocumentTypeTest, Duplicate) {
  scoped_refptr<DocumentType> doctype = new DocumentType(
      document_, base::Token("name"), "public_id", "system_id");
  scoped_refptr<DocumentType> new_doctype =
      doctype->Duplicate()->AsDocumentType();
  ASSERT_TRUE(new_doctype);
  EXPECT_EQ("name", new_doctype->name());
  EXPECT_EQ("public_id", new_doctype->public_id());
  EXPECT_EQ("system_id", new_doctype->system_id());
}

}  // namespace dom
}  // namespace cobalt
